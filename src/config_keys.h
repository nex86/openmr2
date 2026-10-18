#pragma once
struct ConfigKey { const char *legacy, *canonical; };
static const ConfigKey CONFIG_KEYS[] = {
 {"spielverzeichnis","game_directory"},{"grafik_detail","texture_filter"},{"grafikmodus","graphics_mode"},
 {"oldskool_skalierung","retro_scale"},{"oldskool_scale","retro_scale"},{"ganzzahlige_skalierung","integer_scale"},{"vollbild","fullscreen"},
 {"zwischenbilder","interpolation"},{"zwischenbilder_experimentell","interpolation"},{"bildausgabe_rennen","frame_rate"},
 {"debug_bildausgabe","frame_rate"},{"soundeffekte","sound_effects"},{"musik_rennen","race_music"},
 {"musik_hauptmenue","menu_music"},{"geraeusche_menues","menu_sounds"},{"lance_boyle_videos","lance_boyle_videos"},
 {"runden","laps"},{"steuerung","controls"},{"pad_gas","pad_accelerate"},{"pad_bremse","pad_brake"},
 {"pad_links","pad_left"},{"pad_rechts","pad_right"},{"pad_frontwaffe","pad_fire_front"},{"pad_heckwaffe","pad_fire_rear"},
 {"pad_frontwaffe_wechseln","pad_switch_front"},{"pad_heckwaffe_wechseln","pad_switch_rear"},
 {"debug_renninfos","debug_race_info"},{"debug_fahrbahn_markierungen","debug_road_markers"},
 {"debug_ohne_gegner","debug_no_opponents"},{"debug_bild_4_3_gestreckt","debug_stretch"},
 {"debug_alte_kamera","debug_legacy_camera"},{"debug_videos_ueberspringen","debug_skip_videos"},
 {"fahrmodell","simulation_model"},{"ki_notfallhilfe","ai_rescue"},{"cheat_geld","cheat_money"},
 {"cheat_munition","cheat_ammo"},{"cheat_waffen_und_wagen","cheat_weapons_and_cars"},{"cheat_kein_schaden","cheat_no_damage"}
};
static const char *cfg_english_key(const char *key) {
 for(unsigned i=0;i<sizeof CONFIG_KEYS/sizeof CONFIG_KEYS[0];i++)if(!str_icmp(key,CONFIG_KEYS[i].legacy))return CONFIG_KEYS[i].canonical;
 return key;
}
