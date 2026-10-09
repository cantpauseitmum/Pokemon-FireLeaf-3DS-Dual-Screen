import sys
import json
import re
import argparse
import os
import glob

def main():
    parser = argparse.ArgumentParser(description="Ekstraktor symboli i map dla portu FireRed 3DS")
    parser.add_argument('--mapfile', required=True, help='Sciezka do pliku pokefirered.map')
    parser.add_argument('--maps_dir', default='data/maps', help='Folder z mapami FireRed')
    parser.add_argument('--out', required=True, help='Sciezka wyjsciowa pliku JSON')
    args = parser.parse_args()

    if not os.path.exists(args.mapfile):
        print(f"Błąd: Plik mapy '{args.mapfile}' nie istnieje. Skompiluj najpierw ROM GBA.")
        sys.exit(1)

    # 1. Ekstrakcja symboli fizycznych z ROM
    pattern = re.compile(r'^\s+(0x08[0-9a-fA-F]+)\s+([a-zA-Z0-9_]+)$')
    offsets = {}
    
    print("[1/2] Skanowanie pokefirered.map w poszukiwaniu symboli...")
    with open(args.mapfile, 'r', encoding='utf-8') as f:
        for line in f:
            match = pattern.match(line)
            if match:
                addr_str, symbol = match.groups()
                # Zmiana adresu z pamięci GBA na wskaźnik pliku binarnego
                addr = int(addr_str, 16) - 0x08000000
                offsets[symbol] = addr

    # 2. Ekstrakcja nagłówków map i kafelków Kanto (gMapHeader) z plików JSON projektu pokefirered
    print("[2/2] Parsowanie map Kanto (gMapHeader i layouts)...")
    map_data = {}
    
    if os.path.exists(args.maps_dir):
        # FireRed posiada plik map_groups.json
        map_groups_path = os.path.join(args.maps_dir, 'map_groups.json')
        if os.path.exists(map_groups_path):
            with open(map_groups_path, 'r') as f:
                groups_json = json.load(f)
                
            # Dla każdej mapy w Kanto próbujemy odczytać jej układ (Layout)
            for group_name, maps_list in groups_json.get("group_order", {}).items():
                for map_name in maps_list:
                    map_json_path = os.path.join(args.maps_dir, map_name, 'map.json')
                    if os.path.exists(map_json_path):
                        with open(map_json_path, 'r') as mf:
                            m_data = json.load(mf)
                            layout = m_data.get("layout", "UNKNOWN_LAYOUT")
                            
                            # Dodajemy wirtualny wpis symboliczny z metadanymi Voxel Buildera
                            map_data[f"gMapHeader_{map_name}"] = {
                                "layout": layout,
                                "id": m_data.get("id"),
                                "bgm": m_data.get("music"),
                                "connections": m_data.get("connections", [])
                            }
    else:
        print("Ostrzeżenie: Nie znaleziono folderu 'data/maps'. Pomijam ekstrakcję Map Kanto.")

    # 3. Zapis do wielkiego pliku offsets_firered.json
    output_data = {
        "symbols": offsets,
        "kanto_maps": map_data
    }
    
    with open(args.out, 'w', encoding='utf-8') as f:
        json.dump(output_data, f, indent=4)
        
    print(f"[Ekstraktor] Sukces! Wyeksportowano {len(offsets)} symboli i {len(map_data)} map Kanto do '{args.out}'.")

if __name__ == '__main__':
    main()
