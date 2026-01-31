# Projet : Four de Refusion de Précision (RP2040, FreeRTOS, LVGL)

## 1. Objectif Global

Transformer un four via un contrôleur basé sur **RP2040** en une station de refusion performante. Le système se distingue par :

* Un **suivi thermique haute précision** via capteurs I2C dédiés (MCP9600).
* Une gestion **multi-zones** (2 éléments chauffants pilotables indépendamment).
* Une architecture logicielle **FreeRTOS** robuste (séparation UI / Contrôle / Sécurité).
* Des **profils de refusion flexibles** (nombre illimité de paliers/segments via SD).
* Une interface graphique (UI) fluide sous **LVGL**.

## 2. Matériel & Interfaces (Mise à jour Câblage Réel)

### 2.1 Cœur & Bus

* **MCU** : Raspberry Pi RP2040 (Dual-core ARM Cortex-M0+ @ 133MHz).
* **Bus SPI0** (Partagé) : Écran TFT + Carte SD. Nécessite une gestion rigoureuse des Chip Select (CS) et du timing.
* **Bus I2C** : Capteurs de température MCP9600 (Adresses distinctes requises).
* **Bus 1-Wire** : Sonde d'ambiance/calibration DS18B20.

### 2.2 Entrées Utilisateur (HMI)

Le panneau de commande dispose de **4 boutons physiques** et **1 encodeur rotatif** :

* **Boutons Nav** : 3 boutons dédiés (GPIO 0, 1, 2) pour actions rapides (Start, Stop, Menu) ou navigation contextuelle.
* **Encodeur** : Rotation (GPIO 6, 7) + Bouton intégré (GPIO 8) pour le réglage fin des valeurs.

### 2.3 Affichage & Feedback

* **Écran** : Module SPI (ex: ILI9341, ST7789) avec contrôle complet (Backlight, DC, Reset).
* **LEDs Statut** : Ruban/Module **WS2812B** (Smart LED) sur GPIO 9 pour coder l'état (Bleu=Cool, Orange=Preheat, Rouge=Reflow, Vert=Complete).
* **Buzzer** : Buzzer actif ou passif (PWM) sur GPIO 28 pour alarmes et bips IHM.

### 2.4 Thermométrie & Puissance

* **Capteur T1 (Principal)** : MCP9600 (Thermocouple convertisseur I2C). Haute précision, compensation de soudure froide intégrée.
* **Capteur T2 (Optionnel)** : MCP9600 secondaire. Le système doit scanner l'I2C au démarrage ; si absent, le four fonctionne en mode "Single Sensor".
* **Sécurité Hardware** : Les broches d'alerte (Alert 1-4) des MCP9600 sont câblées aux GPIOs RP2040. Cela permet de déclencher des interruptions immédiates en cas de surchauffe, sans attendre la boucle de lecture I2C.
* **Actionneurs Chauffe** :
* **SSR 1 (GPIO 26)** : Élément chauffant principal.
* **SSR 2 (GPIO 27)** : Élément chauffant secondaire (Boost ou zone distincte).



---

## 3. Mapping GPIO Définitif

Ce tableau respecte scrupuleusement le fichier `Extrait du cablage réal.csv`.

| GPIO | Fonction Label | Type Signal | Description Technique / Usage |
| --- | --- | --- | --- |
| **0** | BTN1 | Input (IRQ) | Bouton Façade 1 (Navigation/Action) |
| **1** | BTN2 | Input (IRQ) | Bouton Façade 2 (Navigation/Action) |
| **2** | BTN3 | Input (IRQ) | Bouton Façade 3 (Navigation/Action) |
| **3** | SD_CD | Input | **SD Card Detect**. Low = Carte présente. |
| **4** | I2C_SDA | I2C Data | Bus I2C pour MCP9600 (Sensor 1 & 2). |
| **5** | I2C_SCL | I2C Clock | Bus I2C pour MCP9600 (Sensor 1 & 2). |
| **6** | ROT_DT | Input (IRQ) | Encodeur Phase B (Data). |
| **7** | ROT_CLK | Input (IRQ) | Encodeur Phase A (Clock). |
| **8** | ROT_BTN | Input (IRQ) | Bouton poussoir de l'encodeur. |
| **9** | WS_LED | Output (Data) | Signal **WS2812B** (Neopixel) pour feedback visuel. |
| **10** | T1_ALT4 | Input (IRQ) | MCP9600 T1 Alert 4 (Seuil critique Hard). |
| **11** | T1_ALT3 | Input (IRQ) | MCP9600 T1 Alert 3. |
| **12** | T1_ALT2 | Input (IRQ) | MCP9600 T1 Alert 2. |
| **13** | T1_ALT1 | Input (IRQ) | MCP9600 T1 Alert 1 (Seuil pré-alerte). |
| **14** | T2_ALT4 | Input (IRQ) | MCP9600 T2 Alert 4 (Si T2 présent). |
| **15** | T2_ALT3 | Input (IRQ) | MCP9600 T2 Alert 3. |
| **16** | T2_ALT2 | Input (IRQ) | MCP9600 T2 Alert 2. |
| **17** | T2_ALT1 | Input (IRQ) | MCP9600 T2 Alert 1. |
| **18** | SPI_SCK | SPI0 Clock | Horloge partagée Écran + SD. |
| **19** | SPI_MOSI | SPI0 TX | Données sortantes (Vers Écran + SD). |
| **20** | SPI_MISO | SPI0 RX | Données entrantes (Depuis SD uniquement). |
| **21** | SCR_CS | Output | **Chip Select Écran**. |
| **22** | SD_CS | Output | **Chip Select SD Card** (Note: Label SD_DAT3/CS). |
| **23** | SCR_BL | Output (PWM) | Backlight écran (dimming possible). |
| **24** | SCR_RST | Output | Reset Hardware Écran. |
| **25** | SCR_DC | Output | Data/Command Écran. |
| **26** | HEAT1 | Output (PWM) | Pilotage SSR 1 (Résistance principale). |
| **27** | HEAT2 | Output (PWM) | Pilotage SSR 2 (Résistance secondaire). |
| **28** | BUZZER | Output (PWM) | Buzzer (Tones passifs ou Active High). |
| **29** | 1-WIRE | In/Out | Bus OneWire pour DS18B20 (Ambiante/Ref). |

---

## 4. Fonctionnalités Logicielles Avancées

### 4.1 Profils Thermiques Dynamiques ("Flexible Steps")

Contrairement à une structure rigide (Preheat/Soak/Reflow), le moteur de profil lira une liste de **segments**. Cela permet de créer des profils complexes (ex: recuit lent, double refusion, séchage PCB).

* **Types de segments supportés** :
* `RAMP` : Atteindre une T° cible avec une pente donnée (°C/s).
* `STEP` : Saut immédiat de consigne (step response).
* `HOLD` : Maintenir la T° pendant X secondes.


* **Chargement** : Parsing JSON depuis la carte SD.

### 4.2 Gestion Hardware "Safe"

* **Détection T2** : Au boot, ping I2C sur l'adresse de T2. Si ACK → Activation T2. Sinon → Ignorer T2 et masquer les infos T2 sur l'UI.
* **Interruptions MCP9600** : Configuration des registres MCP9600 pour trigger les pins ALT (GPIO 10-17) si T° > 260°C (hard limit). Une ISR sur ces pins coupe immédiatement les SSR, bypassant le RTOS en cas de plantage tâche.

### 4.3 Contrôle PID & PWM

* **PWM Lente** : Les SSR Zéro-crossing n'aiment pas le PWM rapide. On utilisera un PWM logiciel ou hardware à très basse fréquence (ex: 2Hz à 5Hz) ou un algorithme de Bresenham sur une base de temps de 100ms.
* **Dual PID** : Possibilité d'avoir des paramètres PID différents pour SSR1 et SSR2, ou de coupler SSR2 en mode "Esclave" (ex: SSR2 = 80% de SSR1 pour homogénéiser).

---

## 5. Architecture Système (FreeRTOS)

L'absence de Servo simplifie la sortie, mais l'ajout de la gestion SPI partagée et des interruptions MCP demande de la rigueur.

### 5.1 Tâches (Tasks)

| Priorité | Tâche | Stack | Description |
| --- | --- | --- | --- |
| **High (5)** | `Alert_Handling` | Minimal | Traitement des interruptions GPIO (MCP9600 ALT pins) et Watchdog. Arrêt d'urgence. |
| **High (4)** | `Sensor_Poller` | 3 KB | Lecture I2C (MCP9600) et 1-Wire. Conversion des données brutes. Gestion des erreurs de lecture. |
| **Med (3)** | `PID_Loop` | 2 KB | Calcul de l'erreur, PID, output PWM vers SSRs. Cycle fixe (ex: 200ms). |
| **Med (3)** | `App_Logic` | 4 KB | Machine d'états (Idle, Run, Cooldown). Orchestrateur principal. Parseur JSON. |
| **Low (2)** | `GUI_Task` | 8 KB | Gestion LVGL, rafraîchissement écran, lecture inputs (Queue events). |
| **Low (1)** | `Disk_Logger` | 4 KB | Écriture asynchrone des logs CSV sur SD (pour ne pas bloquer les tâches critiques). |

### 5.2 Gestion des Ressources (Mutex & Queues)

* **`mtx_SPI0` (CRITIQUE)** : L'écran et la SD sont sur le même bus.
* La tâche `GUI_Task` doit prendre ce mutex avant de dessiner.
* La tâche `Disk_Logger` (et le chargeur de profil `App_Logic`) doit prendre ce mutex avant tout accès fichier.
* *Stratégie* : Utiliser le DMA pour l'écran pour minimiser le temps de blocage du CPU, mais le Mutex reste obligatoire pour l'accès bus.


* **`mtx_I2C`** : Protection d'accès aux capteurs MCP9600.
* **`q_SensorData`** : Structure contenant `{temp1, temp2, temp_amb, status_flags}` envoyée par *Sensor_Poller* vers *PID_Loop* et *GUI*.

---

## 6. Structure des Fichiers (Carte SD)

### 6.1 Format de Profil (`/profiles/sac305.json`)

Structure mise à jour pour permettre un nombre illimité d'étapes.

```json
{
  "meta": {
    "name": "SAC305 Standard",
    "description": "Profil sans plomb classique",
    "author": "User"
  },
  "safety": {
    "max_temp": 255,
    "max_slope": 3.0
  },
  "segments": [
    { "type": "ramp", "end_temp": 150, "slope": 1.5, "note": "Preheat" },
    { "type": "hold", "duration_s": 90,  "temp": 150, "note": "Soak" },
    { "type": "ramp", "end_temp": 217, "slope": 2.5, "note": "Ramp to Reflow" },
    { "type": "ramp", "end_temp": 245, "slope": 1.0, "note": "Peak Reflow" },
    { "type": "hold", "duration_s": 20,  "temp": 245, "note": "Liquid Time" },
    { "type": "ramp", "end_temp": 50,  "slope": -2.0, "note": "Cooling" }
  ]
}

```

### 6.2 Fichier de Configuration (`/config/system.json`)

```json
{
  "hardware": {
    "enable_sensor2_check": true,
    "screen_orientation": 1,
    "buzzer_volume": 80
  },
  "pid_params": {
    "ssr1": { "kp": 15.0, "ki": 0.05, "kd": 80.0 },
    "ssr2": { "kp": 10.0, "ki": 0.05, "kd": 50.0 }
  },
  "calibration": {
    "t1_offset": -1.5,
    "t2_offset": 0.0
  }
}

```

---

## 7. Machine d'États Révisée

1. **INIT** : Setup GPIO, détection T2 (I2C), montage SD (SD_CD check), init écran.
2. **IDLE** : Attente utilisateur. Lecture T° ambiante. Affichage liste profils.
3. **PRE_CHECK** : Vérification intégrité capteurs (pas de court-circuit MCP9600), porte fermée (si capteur ajouté futur), température de départ < 50°C.
4. **RUNNING** : Exécution du tableau `segments`.
* Calcul de la `Target_Temp` courante par interpolation temporelle.
* PID calcule `% Power`.
* Update Graphique.


5. **COOLDOWN** : Fin du profil ou annulation manuelle. SSR OFF. Affichage "HOT" en rouge/orange sur LED WS2812B tant que T > 50°C. Buzzer intermittent.
6. **FAULT** : Déclenché par ISR (Pins ALT) ou timeout logiciel. SSR OFF hard (GPIO low). Buzzer continu. Log de l'erreur.

---

## 8. Roadmap Technique & Étapes Clés

1. **Low-Level Drivers (HAL)** :
* Valider le SPI partagé : Dessiner sur l'écran puis écrire un fichier texte sur la SD dans une boucle simple sans corruption.
* Valider I2C MCP9600 : Lire la T° et configurer les registres d'alerte pour tester le déclenchement des GPIO 10-13.


2. **Intégration FreeRTOS Base** :
* Lancer LVGL dans sa tâche.
* Lancer le logger SD dans sa tâche.
* Vérifier que les inputs (Encodeur/Boutons) via interruptions mettent bien à jour l'UI.


3. **Moteur Thermique** :
* Implémenter le parseur de segments JSON.
* Coder la boucle PID.
* Tester avec une consigne fixe (Mode Manuel) avant de lancer un profil.


4. **Raffinement** :
* Logique de détection automatique du Capteur 2.
* Effets LED WS2812B (Dégradés de couleur selon T°).
* Tuning PID.
