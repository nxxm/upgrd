#pragma once

#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/exception/diagnostic_information.hpp> 


namespace shipxx {
  namespace fs = boost::filesystem;

  const auto DONE_FILE = ".shipxx.done";

  /**
   * \return whether the path has been marked done with a DONE_FILE and contains the expected content
   */
  inline bool is_already_done(fs::path path, std::string expected, const fs::path done_file_name = DONE_FILE) {

    fs::path done_file_path = path;
    if (fs::is_directory(path)) {
      done_file_path /= done_file_name;
    } else {
      done_file_path += done_file_name;
    }

    if (fs::exists(done_file_path)) {
      std::ifstream done_file{done_file_path.generic_string().data()};
      done_file.exceptions(std::ios::badbit);
      std::string content;
      done_file >> content;

      return (content == expected);
    } else {
      return false;
    }

  }

  inline void write_already_done(fs::path path, std::string expected, const fs::path done_file_name = DONE_FILE) {
    fs::path done_file_path = path;
    if (fs::is_directory(path)) {
      done_file_path /= done_file_name;
    } else {
      done_file_path += done_file_name;
    }

    fs::remove_all(done_file_path);
    std::fstream done_file{done_file_path.generic_string().data(), std::ios::out | std::ios::in | std::ios::trunc};
    done_file.exceptions(std::ios::badbit);
    done_file << expected;
  };
}
