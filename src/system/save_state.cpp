#include "system/save_state.hpp"
#include "apu/apu.hpp"
#include "cartridge/cartridge.hpp"
#include "core/bus.hpp"
#include "cpu/cpu_6502.hpp"
#include "ppu/ppu.hpp"
#include <chrono>
#include <cstring>
#include <fstream>

namespace nes {

// CRC32 table for checksum calculation
static const uint32_t crc32_table[256] = {
	0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3, 0x0EDB8832,
	0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
	0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7, 0x136C9856, 0x646BA8C0, 0xFD62F97A,
	0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
	0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3,
	0x45DF5C75, 0xDCD60DCF, 0xABD13D59, 0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
	0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB,
	0xB6662D3D, 0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
	0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01, 0x6B6B51F4,
	0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
	0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65, 0x4DB26158, 0x3AB551CE, 0xA3BC0074,
	0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
	0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525,
	0x206F85B3, 0xB966D409, 0xCE61E49F, 0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
	0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615,
	0x73DC1683, 0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
	0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7, 0xFED41B76,
	0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
	0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B, 0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6,
	0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
	0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7,
	0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D, 0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
	0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7,
	0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
	0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45, 0xA00AE278,
	0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
	0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9, 0xBDBDF21C, 0xCABAC28A, 0x53B39330,
	0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
	0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D};

static uint32_t calculate_crc32(const uint8_t *data, size_t length) {
	uint32_t crc = 0xFFFFFFFF;
	for (size_t i = 0; i < length; ++i) {
		crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
	}
	return crc ^ 0xFFFFFFFF;
}

// SaveStateHeader implementation
SaveStateHeader::SaveStateHeader() : version(SAVE_STATE_VERSION), crc32(0), timestamp(0), data_size(0) {
	std::memcpy(magic, SAVE_STATE_MAGIC, 8);
	std::memset(reserved, 0, sizeof(reserved));
}

bool SaveStateHeader::is_valid() const {
	return std::memcmp(magic, SAVE_STATE_MAGIC, 8) == 0 && version == SAVE_STATE_VERSION;
}

// SaveStateManager implementation
SaveStateManager::SaveStateManager(CPU6502 *cpu, PPU *ppu, APU *apu, SystemBus *bus, Cartridge *cartridge)
	: cpu_(cpu), ppu_(ppu), apu_(apu), bus_(bus), cartridge_(cartridge) {
	// Default save directory to "saves" folder next to executable
	save_directory_ = std::filesystem::current_path() / "saves";
}

void SaveStateManager::set_save_directory(const std::filesystem::path &dir) {
	save_directory_ = dir;
}

std::filesystem::path SaveStateManager::get_save_directory() const {
	return save_directory_;
}

std::filesystem::path SaveStateManager::get_slot_path(int slot) const {
	if (slot < 1 || slot > 9) {
		return {};
	}

	// Create filename based on ROM name and slot number
	std::string rom_name = "default";
	if (cartridge_ && cartridge_->is_loaded()) {
		// Get ROM filename without extension
		rom_name = cartridge_->get_rom_filename();
		// Remove .nes extension if present
		if (rom_name.size() > 4 && rom_name.substr(rom_name.size() - 4) == ".nes") {
			rom_name = rom_name.substr(0, rom_name.size() - 4);
		}
	}

	return save_directory_ / (rom_name + "_slot" + std::to_string(slot) + ".vns");
}

bool SaveStateManager::slot_exists(int slot) const {
	auto path = get_slot_path(slot);
	return !path.empty() && std::filesystem::exists(path);
}

std::optional<std::chrono::system_clock::time_point> SaveStateManager::get_slot_timestamp(int slot) const {
	if (!slot_exists(slot)) {
		return std::nullopt;
	}

	auto path = get_slot_path(slot);
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return std::nullopt;
	}

	SaveStateHeader header;
	file.read(reinterpret_cast<char *>(&header), sizeof(header));

	if (!header.is_valid()) {
		return std::nullopt;
	}

	return std::chrono::system_clock::from_time_t(static_cast<std::time_t>(header.timestamp));
}

uint32_t SaveStateManager::calculate_rom_crc32() const {
	if (!cartridge_ || !cartridge_->is_loaded()) {
		return 0;
	}

	// Calculate CRC32 of PRG ROM data
	const auto &prg_rom = cartridge_->get_prg_rom();
	return calculate_crc32(prg_rom.data(), prg_rom.size());
}

// Serialization helper methods
void SaveStateManager::write_uint8(std::vector<uint8_t> &buffer, uint8_t value) {
	buffer.push_back(value);
}

void SaveStateManager::write_uint16(std::vector<uint8_t> &buffer, uint16_t value) {
	buffer.push_back(static_cast<uint8_t>(value & 0xFF));
	buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void SaveStateManager::write_uint32(std::vector<uint8_t> &buffer, uint32_t value) {
	buffer.push_back(static_cast<uint8_t>(value & 0xFF));
	buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
	buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
	buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void SaveStateManager::write_uint64(std::vector<uint8_t> &buffer, uint64_t value) {
	for (int i = 0; i < 8; ++i) {
		buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
	}
}

void SaveStateManager::write_bool(std::vector<uint8_t> &buffer, bool value) {
	buffer.push_back(value ? 1 : 0);
}

void SaveStateManager::write_bytes(std::vector<uint8_t> &buffer, const uint8_t *data, size_t size) {
	buffer.insert(buffer.end(), data, data + size);
}

uint8_t SaveStateManager::read_uint8(const std::vector<uint8_t> &buffer, size_t &offset) {
	return buffer[offset++];
}

uint16_t SaveStateManager::read_uint16(const std::vector<uint8_t> &buffer, size_t &offset) {
	uint16_t value = buffer[offset] | (static_cast<uint16_t>(buffer[offset + 1]) << 8);
	offset += 2;
	return value;
}

uint32_t SaveStateManager::read_uint32(const std::vector<uint8_t> &buffer, size_t &offset) {
	uint32_t value = buffer[offset] | (static_cast<uint32_t>(buffer[offset + 1]) << 8) |
					 (static_cast<uint32_t>(buffer[offset + 2]) << 16) |
					 (static_cast<uint32_t>(buffer[offset + 3]) << 24);
	offset += 4;
	return value;
}

uint64_t SaveStateManager::read_uint64(const std::vector<uint8_t> &buffer, size_t &offset) {
	uint64_t value = 0;
	for (int i = 0; i < 8; ++i) {
		value |= static_cast<uint64_t>(buffer[offset + i]) << (i * 8);
	}
	offset += 8;
	return value;
}

bool SaveStateManager::read_bool(const std::vector<uint8_t> &buffer, size_t &offset) {
	return buffer[offset++] != 0;
}

void SaveStateManager::read_bytes(const std::vector<uint8_t> &buffer, size_t &offset, uint8_t *data, size_t size) {
	std::memcpy(data, buffer.data() + offset, size);
	offset += size;
}

std::vector<uint8_t> SaveStateManager::serialize_state() {
	std::vector<uint8_t> buffer;

	// Reserve space for header (will write at end)
	buffer.resize(sizeof(SaveStateHeader));

	// Serialize CPU state
	if (cpu_) {
		cpu_->serialize_state(buffer);
	}

	// Serialize PPU state
	if (ppu_) {
		ppu_->serialize_state(buffer);
	}

	// Serialize APU state
	if (apu_) {
		apu_->serialize_state(buffer);
	}

	// Serialize memory state (Work RAM, PRG RAM, CHR RAM)
	if (bus_) {
		bus_->serialize_state(buffer);
	}

	// Serialize cartridge/mapper state
	if (cartridge_) {
		cartridge_->serialize_state(buffer);
	}

	// Create and write header at the beginning
	SaveStateHeader header;
	header.crc32 = calculate_rom_crc32();
	header.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	header.data_size = static_cast<uint32_t>(buffer.size() - sizeof(SaveStateHeader));

	std::memcpy(buffer.data(), &header, sizeof(SaveStateHeader));

	return buffer;
}

bool SaveStateManager::deserialize_state(const std::vector<uint8_t> &data) {
	if (data.size() < sizeof(SaveStateHeader)) {
		last_error_ = "Invalid save state: file too small";
		return false;
	}

	// Read and validate header
	SaveStateHeader header;
	std::memcpy(&header, data.data(), sizeof(SaveStateHeader));

	if (!header.is_valid()) {
		last_error_ = "Invalid save state: bad magic number or version";
		return false;
	}

	// Verify ROM CRC32 matches
	uint32_t current_crc = calculate_rom_crc32();
	if (header.crc32 != current_crc) {
		last_error_ = "Save state is for a different ROM";
		return false;
	}

	// Deserialize component states
	size_t offset = sizeof(SaveStateHeader);

	try {
		if (cpu_) {
			cpu_->deserialize_state(data, offset);
		}

		if (ppu_) {
			ppu_->deserialize_state(data, offset);
		}

		if (apu_) {
			apu_->deserialize_state(data, offset);
		}

		if (bus_) {
			bus_->deserialize_state(data, offset);
		}

		if (cartridge_) {
			cartridge_->deserialize_state(data, offset);
		}
	} catch (const std::exception &e) {
		last_error_ = std::string("Deserialization error: ") + e.what();
		return false;
	}

	return true;
}

bool SaveStateManager::save_to_file(const std::filesystem::path &path) {
	// Ensure save directory exists
	std::filesystem::create_directories(path.parent_path());

	// Serialize state
	auto data = serialize_state();

	// Write to file
	std::ofstream file(path, std::ios::binary);
	if (!file) {
		last_error_ = "Failed to open file for writing: " + path.string();
		return false;
	}

	file.write(reinterpret_cast<const char *>(data.data()), data.size());

	if (!file) {
		last_error_ = "Failed to write save state file: " + path.string();
		return false;
	}

	return true;
}

bool SaveStateManager::load_from_file(const std::filesystem::path &path) {
	// Check if file exists
	if (!std::filesystem::exists(path)) {
		last_error_ = "Save state file not found: " + path.string();
		return false;
	}

	// Read file
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		last_error_ = "Failed to open file for reading: " + path.string();
		return false;
	}

	file.seekg(0, std::ios::end);
	size_t file_size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<uint8_t> data(file_size);
	file.read(reinterpret_cast<char *>(data.data()), file_size);

	if (!file) {
		last_error_ = "Failed to read save state file: " + path.string();
		return false;
	}

	// Deserialize
	return deserialize_state(data);
}

bool SaveStateManager::save_to_slot(int slot) {
	if (slot < 1 || slot > 9) {
		last_error_ = "Invalid slot number (must be 1-9)";
		return false;
	}

	auto path = get_slot_path(slot);
	if (path.empty()) {
		last_error_ = "Failed to determine save path for slot";
		return false;
	}

	return save_to_file(path);
}

bool SaveStateManager::load_from_slot(int slot) {
	if (slot < 1 || slot > 9) {
		last_error_ = "Invalid slot number (must be 1-9)";
		return false;
	}

	auto path = get_slot_path(slot);
	if (path.empty()) {
		last_error_ = "Failed to determine save path for slot";
		return false;
	}

	return load_from_file(path);
}

bool SaveStateManager::quick_save() {
	auto path = save_directory_ / "quicksave.vns";
	return save_to_file(path);
}

bool SaveStateManager::quick_load() {
	auto path = save_directory_ / "quicksave.vns";
	return load_from_file(path);
}

} // namespace nes
