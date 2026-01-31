import tkinter as tk
from tkinter import ttk, messagebox, filedialog
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
import json
import numpy as np

# --- MOTEUR DE SIMULATION (Repris et adapté) ---
def simulate_profile(profile_data):
    """Calcule les points (temps, temp) pour le tracé."""
    start_temp = 25.0
    time_points = [0]
    temp_points = [start_temp]
    
    current_time = 0
    current_temp = start_temp
    
    segments = profile_data.get("segments", [])
    
    valid = True
    error_msg = ""

    for i, seg in enumerate(segments):
        seg_type = seg.get("type", "hold")
        
        try:
            if seg_type == "ramp":
                end_temp = float(seg.get("end_temp", current_temp))
                slope = float(seg.get("slope", 1.0))
                
                if slope == 0: slope = 0.1 # Éviter div/0
                
                duration = abs(end_temp - current_temp) / abs(slope)
                
                # Génération points
                steps = max(2, int(duration))
                t_new = np.linspace(current_time, current_time + duration, steps)
                T_new = np.linspace(current_temp, end_temp, steps)
                
                time_points.extend(t_new[1:])
                temp_points.extend(T_new[1:])
                
                current_time += duration
                current_temp = end_temp
                
            elif seg_type == "hold":
                duration = float(seg.get("duration_s", 10.0))
                target = float(seg.get("temp", current_temp))
                
                # Si la cible est diff de l'actuelle (step immédiat avant hold)
                if target != current_temp:
                    time_points.append(current_time)
                    temp_points.append(target)
                    current_temp = target
                
                time_points.append(current_time + duration)
                temp_points.append(current_temp)
                current_time += duration

            elif seg_type == "step":
                target = float(seg.get("temp", current_temp))
                time_points.append(current_time)
                temp_points.append(target)
                current_temp = target

        except Exception as e:
            valid = False
            error_msg = f"Erreur Segment {i+1}: {str(e)}"

    return time_points, temp_points, valid, error_msg

# --- INTERFACE GRAPHIQUE ---
class ReflowEditor(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Configurateur de Profil Refusion RP2040")
        self.geometry("1100x700")

        # Données par défaut
        self.profile = {
            "meta": {"name": "Nouveau Profil", "description": ""},
            "safety": {"max_temp": 255, "max_slope": 3.0},
            "segments": [
                { "type": "ramp", "end_temp": 150, "slope": 1.5, "note": "Preheat" },
                { "type": "hold", "duration_s": 90,  "temp": 150, "note": "Soak" },
                { "type": "ramp", "end_temp": 217, "slope": 2.5, "note": "To Reflow" },
                { "type": "ramp", "end_temp": 245, "slope": 1.0, "note": "Peak" },
                { "type": "hold", "duration_s": 20,  "temp": 245, "note": "Liquid" },
                { "type": "ramp", "end_temp": 50,  "slope": -2.0, "note": "Cool" }
            ]
        }
        
        self.setup_ui()
        self.refresh_graph()

    def setup_ui(self):
        # -- Layout Principal : Panneau Gauche (Contrôles) / Droite (Graph) --
        paned = tk.PanedWindow(self, orient=tk.HORIZONTAL)
        paned.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        left_frame = ttk.Frame(paned, width=400)
        right_frame = ttk.Frame(paned)
        paned.add(left_frame)
        paned.add(right_frame)

        # === GAUCHE : Gestion ===
        
        # 1. Métadonnées
        meta_frame = ttk.LabelFrame(left_frame, text="Info Profil")
        meta_frame.pack(fill=tk.X, padx=5, pady=5)
        
        ttk.Label(meta_frame, text="Nom:").grid(row=0, column=0, sticky="e")
        self.ent_name = ttk.Entry(meta_frame)
        self.ent_name.grid(row=0, column=1, sticky="ew")
        self.ent_name.insert(0, self.profile['meta']['name'])
        self.ent_name.bind("<KeyRelease>", self.update_meta)

        # 2. Liste des Segments
        seg_list_frame = ttk.LabelFrame(left_frame, text="Séquence")
        seg_list_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Toolbar boutons
        btn_frame = ttk.Frame(seg_list_frame)
        btn_frame.pack(fill=tk.X)
        ttk.Button(btn_frame, text="+ Ramp", command=lambda: self.add_segment("ramp")).pack(side=tk.LEFT)
        ttk.Button(btn_frame, text="+ Hold", command=lambda: self.add_segment("hold")).pack(side=tk.LEFT)
        ttk.Button(btn_frame, text="Suppr", command=self.del_segment).pack(side=tk.LEFT)
        ttk.Button(btn_frame, text="▲", width=3, command=lambda: self.move_segment(-1)).pack(side=tk.LEFT)
        ttk.Button(btn_frame, text="▼", width=3, command=lambda: self.move_segment(1)).pack(side=tk.LEFT)

        # Listbox
        self.lst_segments = tk.Listbox(seg_list_frame, height=10, activestyle='none')
        self.lst_segments.pack(fill=tk.BOTH, expand=True, pady=5)
        self.lst_segments.bind("<<ListboxSelect>>", self.on_select_segment)

        # 3. Éditeur de Segment (Dynamique)
        self.edit_frame = ttk.LabelFrame(left_frame, text="Édition Segment")
        self.edit_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # Champs (créés une fois, affichés/masqués selon type)
        self.vars = {
            "type": tk.StringVar(),
            "temp": tk.DoubleVar(),
            "slope": tk.DoubleVar(),
            "duration": tk.DoubleVar(),
            "note": tk.StringVar()
        }
        
        # Grid pour l'éditeur
        self.lbl_type = ttk.Label(self.edit_frame, textvariable=self.vars["type"], font=('bold'))
        self.lbl_type.grid(row=0, column=0, columnspan=2, pady=5)

        # Note
        ttk.Label(self.edit_frame, text="Note:").grid(row=1, column=0, sticky="e")
        e_note = ttk.Entry(self.edit_frame, textvariable=self.vars["note"])
        e_note.grid(row=1, column=1, sticky="ew")
        e_note.bind("<KeyRelease>", self.update_segment_data)

        # Temp Cible
        self.l_temp = ttk.Label(self.edit_frame, text="T° Cible (°C):")
        self.e_temp = ttk.Entry(self.edit_frame, textvariable=self.vars["temp"])
        self.e_temp.bind("<KeyRelease>", self.update_segment_data)

        # Pente (Ramp only)
        self.l_slope = ttk.Label(self.edit_frame, text="Pente (°C/s):")
        self.e_slope = ttk.Entry(self.edit_frame, textvariable=self.vars["slope"])
        self.e_slope.bind("<KeyRelease>", self.update_segment_data)

        # Durée (Hold only)
        self.l_duration = ttk.Label(self.edit_frame, text="Durée (s):")
        self.e_duration = ttk.Entry(self.edit_frame, textvariable=self.vars["duration"])
        self.e_duration.bind("<KeyRelease>", self.update_segment_data)

        # 4. Actions Fichier
        act_frame = ttk.Frame(left_frame)
        act_frame.pack(fill=tk.X, pady=10)
        ttk.Button(act_frame, text="Charger JSON", command=self.load_json).pack(side=tk.LEFT, padx=5)
        ttk.Button(act_frame, text="Sauvegarder JSON", command=self.save_json).pack(side=tk.LEFT, padx=5)

        # === DROITE : Graphique ===
        self.figure = Figure(figsize=(6, 5), dpi=100)
        self.ax = self.figure.add_subplot(111)
        self.canvas = FigureCanvasTkAgg(self.figure, right_frame)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        self.populate_list()

    # --- LOGIQUE ---

    def populate_list(self):
        """Remplit la Listbox avec les noms des segments."""
        selected = self.lst_segments.curselection()
        self.lst_segments.delete(0, tk.END)
        for i, seg in enumerate(self.profile["segments"]):
            t = seg.get("type", "?").upper()
            n = seg.get("note", "")
            if t == "RAMP":
                detail = f"-> {seg.get('end_temp')}°C ({seg.get('slope')}°/s)"
            elif t == "HOLD":
                detail = f"@{seg.get('temp', '-')}°C ({seg.get('duration_s')}s)"
            else:
                detail = ""
            self.lst_segments.insert(tk.END, f"{i+1}. {t} : {n} [{detail}]")
        
        if selected: # Restore selection
            try: self.lst_segments.selection_set(selected)
            except: pass

    def on_select_segment(self, event):
        """Affiche les champs adéquats quand on clique sur une ligne."""
        sel = self.lst_segments.curselection()
        if not sel: return
        
        idx = sel[0]
        seg = self.profile["segments"][idx]
        stype = seg.get("type", "hold")
        
        self.vars["type"].set(stype.upper())
        self.vars["note"].set(seg.get("note", ""))
        
        # Reset grid
        self.l_temp.grid_forget(); self.e_temp.grid_forget()
        self.l_slope.grid_forget(); self.e_slope.grid_forget()
        self.l_duration.grid_forget(); self.e_duration.grid_forget()

        row_idx = 2
        if stype == "ramp":
            self.vars["temp"].set(seg.get("end_temp", 0))
            self.vars["slope"].set(seg.get("slope", 1.0))
            
            self.l_temp.grid(row=row_idx, column=0, sticky="e"); self.e_temp.grid(row=row_idx, column=1)
            row_idx+=1
            self.l_slope.grid(row=row_idx, column=0, sticky="e"); self.e_slope.grid(row=row_idx, column=1)
            
        elif stype == "hold":
            self.vars["temp"].set(seg.get("temp", 0))
            self.vars["duration"].set(seg.get("duration_s", 0))

            self.l_temp.grid(row=row_idx, column=0, sticky="e"); self.e_temp.grid(row=row_idx, column=1)
            row_idx+=1
            self.l_duration.grid(row=row_idx, column=0, sticky="e"); self.e_duration.grid(row=row_idx, column=1)

    def update_segment_data(self, event=None):
        """Enregistre les modifs des Entry vers le dictionnaire profile."""
        sel = self.lst_segments.curselection()
        if not sel: return
        idx = sel[0]
        seg = self.profile["segments"][idx]
        stype = seg.get("type")

        # Mise à jour safe
        try:
            seg["note"] = self.vars["note"].get()
            if stype == "ramp":
                seg["end_temp"] = self.vars["temp"].get()
                seg["slope"] = self.vars["slope"].get()
            elif stype == "hold":
                seg["temp"] = self.vars["temp"].get()
                seg["duration_s"] = self.vars["duration"].get()
        except tk.TclError:
            pass # L'utilisateur est en train de taper un truc vide ou invalide

        self.populate_list() # Pour mettre à jour les labels dans la liste
        self.lst_segments.selection_set(idx) # Garder la sélection
        self.refresh_graph()

    def update_meta(self, event=None):
        self.profile['meta']['name'] = self.ent_name.get()
        self.refresh_graph()

    def add_segment(self, stype):
        new_seg = {}
        if stype == "ramp":
            new_seg = { "type": "ramp", "end_temp": 100, "slope": 1.0, "note": "New Ramp" }
        else:
            new_seg = { "type": "hold", "duration_s": 30, "temp": 100, "note": "New Soak" }
        
        self.profile["segments"].append(new_seg)
        self.populate_list()
        self.lst_segments.selection_set(tk.END)
        self.on_select_segment(None)
        self.refresh_graph()

    def del_segment(self):
        sel = self.lst_segments.curselection()
        if sel:
            del self.profile["segments"][sel[0]]
            self.populate_list()
            self.refresh_graph()

    def move_segment(self, direction):
        sel = self.lst_segments.curselection()
        if not sel: return
        i = sel[0]
        if direction == -1 and i > 0:
            self.profile["segments"][i], self.profile["segments"][i-1] = self.profile["segments"][i-1], self.profile["segments"][i]
            self.populate_list()
            self.lst_segments.selection_set(i-1)
        elif direction == 1 and i < len(self.profile["segments"]) - 1:
            self.profile["segments"][i], self.profile["segments"][i+1] = self.profile["segments"][i+1], self.profile["segments"][i]
            self.populate_list()
            self.lst_segments.selection_set(i+1)
        self.refresh_graph()

    def refresh_graph(self):
        self.ax.clear()
        
        times, temps, valid, msg = simulate_profile(self.profile)
        
        if valid:
            self.ax.plot(times, temps, 'r-', linewidth=2, label='Target')
            self.ax.fill_between(times, temps, color='red', alpha=0.1)
            self.ax.set_title(self.profile['meta']['name'])
            self.ax.set_xlabel("Temps (s)")
            self.ax.set_ylabel("Température (°C)")
            self.ax.grid(True, linestyle='--')
            
            # Limite max
            max_t = self.profile['safety']['max_temp']
            self.ax.axhline(max_t, color='orange', linestyle=':', label='Max Safety')
            
            self.ax.legend()
        else:
            self.ax.text(0.5, 0.5, f"Erreur Profil:\n{msg}", ha='center', va='center', color='red')
        
        self.canvas.draw()

    def save_json(self):
        f = filedialog.asksaveasfile(mode='w', defaultextension=".json", filetypes=[("JSON Files", "*.json")])
        if f is None: return
        json.dump(self.profile, f, indent=2)
        f.close()
        messagebox.showinfo("Succès", "Profil sauvegardé !")

    def load_json(self):
        f = filedialog.askopenfile(mode='r', filetypes=[("JSON Files", "*.json")])
        if f is None: return
        try:
            self.profile = json.load(f)
            self.ent_name.delete(0, tk.END)
            self.ent_name.insert(0, self.profile['meta'].get('name', 'Profil'))
            self.populate_list()
            self.refresh_graph()
        except Exception as e:
            messagebox.showerror("Erreur", f"Impossible de lire le fichier: {e}")

if __name__ == "__main__":
    app = ReflowEditor()
    app.mainloop()