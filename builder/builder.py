import argparse
import json
import hashlib
import sys
import struct
import os
from sappy import SappyExtractor

# Faza 1: Kompleksowy Asset Builder (Zrefaktoryzowany)
# Ten skrypt wyciąga dane z legalnego ROMu GBA.

EXPECTED_SHA1_FIRERED = "e26ee0d44e809351c8ce2d73c7400cdddc40e5c0"
EXPECTED_SHA1_LEAFGREEN = "053eb51829e2fa9e5b223c34e320f7972740bc7b" # Opcjonalnie

def verify_rom(rom_data):
    sha1 = hashlib.sha1(rom_data).hexdigest()
    if sha1 == EXPECTED_SHA1_FIRERED:
        print("[OK] Rozpoznano prawidłowy ROM FireRed 1.0 (Squirrels).")
    elif sha1 == EXPECTED_SHA1_LEAFGREEN:
        print("[OK] Rozpoznano prawidłowy ROM LeafGreen 1.0.")
    else:
        print(f"[Błąd] Zły wariant ROMu! Oczekiwano: {EXPECTED_SHA1_FIRERED}, Otrzymano: {sha1}")
        print("Upewnij się, że używasz czystego FireRed (U) 1.0 (Squirrels) lub LeafGreen (U) 1.0.")
        sys.exit(1)

def extract_asset(rom_data, offset, size):
    return rom_data[offset:offset+size]

def get_dynamic_size(current_offset, sorted_offsets_list, rom_size):
    """
    Kalkuluje precyzyjny rozmiar symbolu poprzez zmierzenie dystansu
    do następnego zarejestrowanego symbolu w pamięci ROM.
    """
    for symbol_name, symbol_offset in sorted_offsets_list:
        if symbol_offset > current_offset:
            return symbol_offset - current_offset
    return rom_size - current_offset

def read_pointer(rom_data, offset):
    ptr = struct.unpack('<I', rom_data[offset:offset+4])[0]
    if ptr >= 0x08000000 and ptr < 0x0A000000:
        return ptr - 0x08000000
    return None

def decompress_lz77(rom_data, start_offset):
    if rom_data[start_offset] != 0x10:
        return None
    
    uncompressed_size = rom_data[start_offset+1] | (rom_data[start_offset+2] << 8) | (rom_data[start_offset+3] << 16)
    dest = bytearray()
    
    src_pos = start_offset + 4
    while len(dest) < uncompressed_size:
        flags = rom_data[src_pos]
        src_pos += 1
        
        for i in range(7, -1, -1):
            if len(dest) >= uncompressed_size:
                break
                
            is_compressed = (flags >> i) & 1
            if is_compressed:
                b1 = rom_data[src_pos]
                b2 = rom_data[src_pos+1]
                src_pos += 2
                
                length = (b1 >> 4) + 3
                dist = (((b1 & 0x0F) << 8) | b2) + 1
                
                for _ in range(length):
                    dest.append(dest[-dist])
            else:
                dest.append(rom_data[src_pos])
                src_pos += 1
                
    return bytes(dest)



def build_pak(rom_path, out_path, offsets_path):
    print(f"--- FireRed 3DS Builder ---")
    
    print(f"Wczytywanie ROMu GBA: {rom_path}")
    with open(rom_path, 'rb') as f:
        rom_data = f.read()
        
    # FAZA 1: Rygorystyczna Walidacja
    verify_rom(rom_data)
        
    print(f"Wczytywanie mapy offsetów: {offsets_path}")
    with open(offsets_path, 'r') as f:
        json_data = json.load(f)
        
    # extract_symbols.py zapisuje symbole w kluczu "symbols"
    symbols_dict = json_data.get("symbols", {})
    if not symbols_dict:
        # Fallback dla starego mock_json (łączymy wszystkie słowniki)
        symbols_dict = {}
        if "graphics" in json_data: symbols_dict.update({k: v["offset"] for k, v in json_data["graphics"].items()})
        if "palettes" in json_data: symbols_dict.update({k: v["offset"] for k, v in json_data["palettes"].items()})
        if "audio" in json_data: symbols_dict.update({k: v["offset"] for k, v in json_data["audio"].items()})
        
    # Sortowanie symboli do dynamicznego przeliczania wielkości danych
    sorted_symbols = sorted(symbols_dict.items(), key=lambda x: x[1])
    
    print(f"Rozpoczynanie ekstrakcji i kompresji zasobów do: {out_path}")
    
    with open(out_path, 'wb') as pak:
        for symbol_name, offset in symbols_dict.items():
            # Pomijanie symboli niebędących grafiką / audio
            if not isinstance(offset, int):
                continue
                
            is_lz77 = False
            
            # Wektoryzacja logiki ekstrakcji
            if symbol_name.startswith("gMonFrontPic_") or symbol_name.startswith("gMonBackPic_"):
                is_lz77 = True
                print(f" -> Kopiowanie Sprite'a Pokemona: {symbol_name}")
                
            elif symbol_name.startswith("gTrainerFrontPic_") or symbol_name.startswith("gTrainerBackPic_"):
                is_lz77 = True
                print(f" -> Kopiowanie Sprite'a Trenera: {symbol_name}")
                
            elif symbol_name.startswith("gObjectEventPic_"):
                is_lz77 = False
                print(f" -> Kopiowanie Sprite'a Mapy: {symbol_name}")
                
            elif symbol_name.startswith("gBattleTerrainPalette_"):
                is_lz77 = False
                print(f" -> Kopiowanie Tła Bitwy: {symbol_name}")

            elif symbol_name.startswith("gMenuWindow") or "MessageBox" in symbol_name:
                is_lz77 = False
                print(f" -> Kopiowanie Ramki UI: {symbol_name}")
                
            elif symbol_name.startswith("gBgm") or symbol_name.startswith("gSe") or "VoiceTable" in symbol_name or symbol_name == "gSongTable":
                is_lz77 = False
                # Pomiń zwykłe kopiowanie, zrobimy to przez SappyExtractor
                print(f" -> Kopiowanie Zasobu Audio (Sappy): {symbol_name}")
                if symbol_name == "gSongTable":
                    sappy = SappyExtractor(rom_data)
                    # W FireRed jest ok. 156-250 utworów, weźmy 256
                    sappy.extract_song_table(offset + 0x08000000, num_songs=256)
                    for s_offset, (s_name, s_data) in sappy.get_bundled_audio_data().items():
                        name_bytes = s_name.encode('utf-8')
                        pak.write(struct.pack('<B', len(name_bytes)))
                        pak.write(name_bytes)
                        pak.write(struct.pack('<I', len(s_data)))
                        pak.write(s_data)
                continue
                
            else:
                # Pomijamy kod wykonywalny C
                continue
                
            # FAZA 1: Dynamiczny rozmiar dla danych nieskompresowanych
            if is_lz77:
                raw_gfx = decompress_lz77(rom_data, offset)
                if raw_gfx is None:
                    print(f"   [Ostrzeżenie] {symbol_name} nie posiada nagłówka LZ77! Pomijam.")
                    continue
            else:
                size = get_dynamic_size(offset, sorted_symbols, len(rom_data))
                
                # Zabezpieczenie przed gigantycznymi rozmiarami z powodu brakujących symboli w przestrzeni danych
                if size > 65536:
                    print(f"   [Ostrzeżenie] Dynamiczny rozmiar dla {symbol_name} wynosi {size} bajtów. Przycinam do limitu 64KB.")
                    size = 65536
                    
                raw_gfx = extract_asset(rom_data, offset, size)
            
            # Pakowanie zasobu do .pak
            name_bytes = symbol_name.encode('utf-8')
            pak.write(struct.pack('<B', len(name_bytes)))
            pak.write(name_bytes)
            
            pak.write(struct.pack('<I', len(raw_gfx)))
            pak.write(raw_gfx)
            
    print(f"Pomyślnie wygenerowano plik {out_path}!")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="FireRed 3DS Asset Builder (Refactored)")
    parser.add_argument("--rom", required=True, help="Ścieżka do czystego pliku Pokemon FireRed.gba")
    parser.add_argument("--out", default="pokefirered_assets.pak", help="Plik wynikowy paczki (.pak)")
    parser.add_argument("--offsets", default="offsets_firered.json", help="Mapa offsetów ROMu")
    
    args = parser.parse_args()
    build_pak(args.rom, args.out, args.offsets)
