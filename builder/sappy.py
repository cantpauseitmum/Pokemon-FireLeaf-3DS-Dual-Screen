import struct

class SappyExtractor:
    """
    Parser silnika Sappy Audio Engine.
    Rekursywnie analizuje struktury banków instrumentów (VoiceGroup),
    nagrań (Samples) oraz sekwencji MIDI wyciągając pełen kod dźwiękowy
    z oryginalnego ROMu FireRed.
    """
    def __init__(self, rom_data):
        self.rom_data = rom_data
        self.extracted_chunks = {}
        self.parsed_pointers = set()

    def read_u32(self, offset):
        return struct.unpack('<I', self.rom_data[offset:offset+4])[0]

    def read_u8(self, offset):
        return self.rom_data[offset]

    def is_valid_ptr(self, ptr):
        return 0x08000000 <= ptr < 0x0A000000

    def to_offset(self, ptr):
        return ptr - 0x08000000

    def extract(self, name, offset, size):
        if offset in self.parsed_pointers:
            return
        self.parsed_pointers.add(offset)
        self.extracted_chunks[offset] = (name, self.rom_data[offset:offset+size])

    def parse_sample(self, sample_ptr, name_hint=""):
        offset = self.to_offset(sample_ptr)
        if offset in self.parsed_pointers:
            return
            
        sample_size = self.read_u32(offset + 12)
        if sample_size > 0 and sample_size < 1024 * 1024:
            total_size = 16 + sample_size
            self.extract(f"SappySample_{offset:08X}_{name_hint}", offset, total_size)
            self.parsed_pointers.add(offset)

    def parse_instrument_bank(self, bank_ptr, num_instruments=128):
        offset = self.to_offset(bank_ptr)
        if offset in self.parsed_pointers:
            return
            
        self.extract(f"SappyVoiceGroup_{offset:08X}", offset, 12 * num_instruments)
        self.parsed_pointers.add(offset)
        
        for i in range(num_instruments):
            inst_offset = offset + (i * 12)
            inst_type = self.read_u8(inst_offset)
            
            if inst_type in (0, 8):
                sample_ptr = self.read_u32(inst_offset + 4)
                if self.is_valid_ptr(sample_ptr):
                    self.parse_sample(sample_ptr, f"Inst{i}")
                    
            elif inst_type == 64:
                drum_ptr = self.read_u32(inst_offset + 4)
                if self.is_valid_ptr(drum_ptr):
                    self.parse_instrument_bank(drum_ptr, 128)

            elif inst_type == 128:
                multi_ptr = self.read_u32(inst_offset + 4)
                if self.is_valid_ptr(multi_ptr):
                    multi_offset = self.to_offset(multi_ptr)
                    self.extract(f"SappyKeymap_{multi_offset:08X}", multi_offset, 128)

    def parse_song(self, song_ptr, song_name=""):
        offset = self.to_offset(song_ptr)
        if offset in self.parsed_pointers:
            return
            
        num_tracks = self.read_u8(offset)
        header_size = 8 + (num_tracks * 4)
        self.extract(f"SongHeader_{song_name}", offset, header_size)
        self.parsed_pointers.add(offset)
        
        bank_ptr = self.read_u32(offset + 4)
        if self.is_valid_ptr(bank_ptr):
            self.parse_instrument_bank(bank_ptr)
            
        for i in range(num_tracks):
            track_ptr = self.read_u32(offset + 8 + (i * 4))
            if self.is_valid_ptr(track_ptr):
                t_offset = self.to_offset(track_ptr)
                if t_offset not in self.parsed_pointers:
                    self.extract(f"SongTrack_{song_name}_{i}", t_offset, 4096)
                    self.parsed_pointers.add(t_offset)

    def extract_song_table(self, table_ptr, num_songs=256):
        offset = self.to_offset(table_ptr)
        self.extract("gSongTable", offset, 8 * num_songs)
        
        for i in range(num_songs):
            song_ptr = self.read_u32(offset + (i * 8))
            if self.is_valid_ptr(song_ptr):
                self.parse_song(song_ptr, f"Song{i}")

    def get_bundled_audio_data(self):
        return self.extracted_chunks
