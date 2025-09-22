#include "header_writer.h"
#include <format>
#include <sstream>
#include <stdexcept>

header_writer::header_writer(const std::filesystem::path& path,
                             const std::string& guard, bool use_stdint,
                             const std::string& spacename)
    : _fstream(path), _using_stdint(use_stdint) {
  if (!_fstream.is_open()) {
    throw std::runtime_error("Could not open file");
  }

  write(std::format("#ifndef {0}\n#define {0}\n", guard));
  if (_using_stdint) {
    write("#include <cstdint>\n");
    _byte_type = "std::uint8_t";
  } else {
    write("static_assert(sizeof(unsigned char) == 1, \"unsigned char must be 1 "
          "byte in size\");");
    _byte_type = "unsigned char";
  }

  if (spacename != "") {
    write(std::format("namespace {} {{", spacename));
    _has_namespace = true;
  }
}

header_writer::~header_writer() { header_writer::close(); }

bool header_writer::is_open() const { return _fstream.is_open(); }

bool header_writer::using_stdint() const { return _using_stdint; }

const std::string& header_writer::byte_type() const { return _byte_type; }

void header_writer::write(const std::string& data) {
  _fstream.write(data.c_str(), data.size());
}

void header_writer::write_variable(const std::string& type,
                                   const std::string& name,
                                   const std::string& value, bool constant) {
  std::string constant_string = "";
  if (constant) {
    constant_string = "constexpr ";
  }
  write(std::format("{}{} {}={};", constant_string, type, name, value));
}

void header_writer::write_byte_array(const std::string& name,
                                     const std::uint8_t* data, std::size_t size,
                                     bool constant) {
  std::stringstream bytes_stream;
  for (std::size_t i = 0; i < size; i++) {
    bytes_stream << static_cast<unsigned int>(data[i]) << ',';
  }

  std::string constant_string = "";
  if (constant) {
    constant_string = "constexpr ";
  }
  write(std::format("{}{} {}[{}]={{{}}};", constant_string, _byte_type, name,
                    size, bytes_stream.str()));
}

void header_writer::close() {
  // prevent double destruction
  if (!_is_closed) {
    if (_has_namespace) {
      write("}");
    }
    write("\n#endif");
    _fstream.close();
    _is_closed = true;
  }
}
