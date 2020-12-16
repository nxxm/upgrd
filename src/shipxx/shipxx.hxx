#pragma once

#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/exception/diagnostic_information.hpp> 
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <cli_widgets/spin_loader.hpp>
#include <xxhr/xxhr.hpp>

#include <shipxx/done.hpp>
#include <shipxx/sha1.hpp>
#include <zip.h>


namespace shipxx {
  namespace fs = boost::filesystem;
  using namespace std::string_literals;

  interprocess_lock(fs::path filename) {
       if (is_directory(filename))
       {
        std::string name_file = "."+(filename.stem()).string()+".lock";
        filename_ = filename / name_file ;
       } else 
       {
         filename_ = filename ;
       }

      fs::create_directories(filename_.parent_path());

      if (!fs::exists(filename_)) {
        std::ofstream file_to_lock((filename_.string()).c_str(), std::ios::trunc);
        file_to_lock.exceptions(std::ios::badbit);
      }

      boost::interprocess::file_lock file_lock_((filename_.string()).c_str());
      boost::interprocess::scoped_lock<boost::interprocess::file_lock> e_lock_(file_lock_);
    }

  private:
    fs::path filename_;
    std::optional<boost::interprocess::file_lock> file_lock_;
    std::optional<boost::interprocess::scoped_lock<boost::interprocess::file_lock>> e_lock_;
  };

  namespace detail {
    inline void extract(fs::path file, fs::path out) {
      auto on_extract_entry = [](const char *filename, void*) {
        //std::cout << "extracted " << filename << std::endl;
        return 0;
      };

      //TODO: handle error case (no more space ...)
      zip_extract(
        file.generic_string().data(),
        out.generic_string().data(),
        on_extract_entry,
        nullptr);
    }
  }

  inline void download(const std::string& url, const fs::path& downloaded_file,
      const std::string& expected_sha1, bool check_sha1 = true,
      std::optional<xxhr::Authentication> auth = std::nullopt) {

    std::cout << "Checking if " << downloaded_file << " is up-to-date." << std::endl;

    using namespace xxhr;
    auto retries_count = 5;
    std::function<void(xxhr::Response&&)> retry_on_fail;

    auto is_downloaded_cache_valid = [&]() {
      if (!check_sha1) {
        return fs::exists(downloaded_file);
      } else if (fs::exists(downloaded_file)) {
        std::ifstream ifs{downloaded_file.generic_string().data(), 
          std::ios::in | std::ios::binary};
        ifs.exceptions(std::ios::badbit);
        ifs.seekg(0, std::ios::end);
        std::string buf(ifs.tellg(), char{});
        ifs.seekg(0, std::ios::beg);
        ifs.read(buf.data(), buf.size());
        ifs.close();

        auto hash = sha1::to_string(sha1::hash(buf));
        return (hash == expected_sha1);
      } else {
        return false;
      }
    };


    auto download = [&]() { 
      std::cout << "Downloading " << url << " as " << downloaded_file << std::endl;

      if ( auth ) {
        GET( url, *auth, on_response = retry_on_fail); 
      } else {
        GET( url, on_response = retry_on_fail); 
      }
    };

    auto store_download = [&](auto&& resp) {
      fs::create_directories(downloaded_file.parent_path());
      std::fstream ofs{downloaded_file.generic_string().data(),
        std::ios::trunc | std::ios::binary | std::ios::out | std::ios::in};
      ofs.exceptions(std::ios::badbit);
      ofs.write(resp.text.data(), resp.text.size());
      ofs.close();
    };

    retry_on_fail = [&](auto&& resp) {
      if ( (resp.error || (resp.status_code != 200)) && (retries_count > 0) ) {
        std::cout << "[" << url << "]: Download error ( i.e. "
          << resp.error << " ), retrying " << retries_count << " times."
          << std::endl; 

        --retries_count;
        download();
      } else if ( !resp.error ) {

        if (!check_sha1) {
          store_download(resp);
        } else {
          auto hash = sha1::to_string(sha1::hash(resp.text));

          if ( (hash != expected_sha1) && (retries_count > 0) ) {
            std::cout << "[" << url << "]: Download error ( i.e. "
              << "exepected hash : " << expected_sha1 << ", received : " << hash 
              << " ), retrying " << retries_count << " times."
              << std::endl; 
            --retries_count;
            download();
          } else {
            store_download(resp);
          }
        }
      } else {
        std::stringstream ss; 
        ss << "[" << url << "]: Download error ( i.e. "
          << resp.error << " ). status code: " << resp.status_code << std::endl; 
        throw std::runtime_error(ss.str());
      }
    };

    if (!is_downloaded_cache_valid()) download();
  }

  /**
   * \brief Check if not in cache, downloads in cache, unpacks and installs with install marker for faster reop given zip archive.
   * \param cache_name Which name to use in downlaod cache within current nxxm distro
   * \param url which source url
   * \param expected_sha1 which expected content hash
   * \param final_dest which destination for the unpacked data
   * \param extract_subdir which subdirectory from zip should be used.
   * \param download_dir which download_dir to use to cache zips
   * \param check_sha1 wether to check sha1 or just trust it to be like provided one. 
   * \param message an optional message which is displayed before the action.
   */
  inline void ship(
      const std::string& cache_name, 
      const std::string& url, 
      const std::string& expected_sha1,
      fs::path final_dest, 
      fs::path extract_subdir,
      std::optional<std::string> message,
      fs::path download_dir,
      bool check_sha1 = true,
      std::optional<xxhr::Authentication> auth = std::nullopt) {
    auto downloaded_zip = download_dir / (cache_name + ".zip"s);
    using namespace xxhr;

    cli_widgets::spin_loader loader{std::cout};
        fs::path path_of_file_for_lock = download_dir / (cache_name + ".zip"s);


    try { 

      //if (!is_already_done(final_dest, expected_sha1)) {

        if (message) {
          std::cout << message.value_or("") + "." << std::endl;
        }

        loader.launch();
        shipxx::interprocess_lock lock{final_dest} ;
   
       
        download(url, downloaded_zip, expected_sha1, check_sha1, auth);
        boost::interprocess::file_lock f_lock((path_of_file_for_lock.string()).c_str());
        boost::interprocess::scoped_lock<boost::interprocess::file_lock> e_lock(f_lock);

        fs::remove_all(download_dir / cache_name);
        detail::extract(downloaded_zip, download_dir / cache_name);

        fs::remove_all(final_dest); 
        fs::create_directories(final_dest.parent_path());
        fs::rename(download_dir / cache_name / extract_subdir, final_dest);
        
        fs::remove_all(download_dir / cache_name);

        //write_already_done(final_dest, expected_sha1);

        loader.stop(message.value_or("") + " done.");
      //} 

    } catch (...) {
      loader.stop("● [err] "s + message.value_or("") + ".", true);
      std::cout << boost::current_exception_diagnostic_information();
      throw std::runtime_error("Impossible to ship "s + cache_name);
    }
  }
}
