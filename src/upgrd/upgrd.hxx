#pragma once

#include <xxhr/xxhr.hpp>
#include <gh/releases.hxx>
#include <regex>
#include <chrono>
#include <boost/predef.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/find.hpp>
#include <boost/filesystem.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/comparison.hpp>
#include <pre/file/string.hpp>

#include <shipxx/shipxx.hxx>

#include <boost/process.hpp>

namespace upgrd {

  using namespace std::string_literals;
  namespace bp = boost::process;
  namespace fs = boost::filesystem;

  struct version_t {
    version_t(const std::string& semver) : semver(semver) {
      std::regex rx_semver("(v|V)?([0-9]+)\\.([0-9]+)\\.([0-9]+).*");
      std::smatch what;
      std::regex_match(semver, what, rx_semver);

      major = std::atoi(what[2].str().data());
      minor = std::atoi(what[3].str().data());
      patch = std::atoi(what[4].str().data());

    }

    size_t major{};
    size_t minor{};
    size_t patch{};

    operator std::string() const {
      return semver;
    }
    private:
    std::string semver;
  };

  using boost::fusion::operator<;
  using boost::fusion::operator==;
  using boost::fusion::operator!=;
  using boost::fusion::operator<;
  using boost::fusion::operator>;
  using boost::fusion::operator<=;
  using boost::fusion::operator>=;
}

BOOST_FUSION_ADAPT_STRUCT(upgrd::version_t, major, minor, patch);

namespace upgrd {

  /**
   * \brief Upgrades your app from Github releases if need be.
   */
  class manager {
    public:

    /**
     * \param uri github name
     * \param semver application version as described in https://semver.org/ 
     */
    manager(const std::string& github_owner, const std::string& github_repo,
        const std::string& semver, fs::path app_path, 
        std::ostream& log)
      :   github_owner_(github_owner)
        , github_repo_(github_repo)
        , current(semver)
        , _app_path(app_path)
        , _log(log)
    {
      fs::create_directories(temp_dir());
    }

    //! \return a temp folder suitable to work in for downloads and preparation
    fs::path temp_dir() {
      return fs::temp_directory_path() / github_owner_ / github_repo_;
    }

    //! \path in temp_dir informing of previous remote version check so that we don't ask server everytime
    fs::path previous_check_file() {
      return fs::temp_directory_path() / (github_owner_ + "."s + github_repo_ + ".checked");
    }

    //! \return the current version installed
    std::string current_version() {
      return std::string(current); 
    }

    /**
     * \brief Tells current version, and checks at github_uri if there is a newer one. 
     *        If that's the case the app get downloaded and replaces this one.
     */
    void propose_upgrade_when_needed() {

      using namespace std::literals::chrono_literals;
      using namespace boost::algorithm;
      try {
        //TODO: write last time tried and check every day once

        std::error_condition dont_throw;
        auto previous_check_str = pre::file::to_string(previous_check_file().generic_string(), dont_throw);
        auto previous_check = 0s;
        if (!previous_check_str.empty()) { previous_check = std::chrono::seconds(std::stol(previous_check_str)); }
        auto yesterday  = (std::chrono::system_clock::now() - 24h).time_since_epoch();

        if (previous_check < yesterday) {

          gh::get_latest_release(github_owner_, github_repo_, [&](gh::releases::release_t&& latest) {
            
            version_t remote{latest.tag_name};
            
            // Write marker
            pre::file::from_string(previous_check_file().generic_string(),
              std::to_string(
                std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                  .count()), dont_throw);

            if (remote > current) {

              auto compatible_asset = std::find_if(latest.assets.begin(), latest.assets.end(), [](auto& asset) {
                std::string asset_lowered_name = to_lower_copy(asset.name);
                #if BOOST_OS_LINUX
                  return asset_lowered_name.find("linux") != std::string::npos;
                #elif BOOST_OS_MACOS
                  return asset_lowered_name.find("macos") != std::string::npos;

                #elif BOOST_OS_WINDOWS
                  return asset_lowered_name.find("windows") != std::string::npos;
  DC0F9620A1CC28C095375453F5BA5546EE76703B
                #endif
              });

              if (compatible_asset != latest.assets.end()) {
                _log << "🆕 " << github_owner_ << "/" << github_repo_ << " version available.\n"
                  << "current version : " << std::string(current) << "\n"
                  << "new version : " << std::string(remote) << "\n\n"
                  
                  << latest.name << "\n"
                  << std::string(latest.name.size(), '^') << "\n"
                  << latest.body 
                  << std::endl;

                char upgrade = 'o';
                _log << "Do you want to perform the upgrade ?." << std::endl;
                while ( (std::tolower(upgrade) != 'y') && (std::tolower(upgrade) != 'n') ) {
                  _log << "[y]es or [n]o ? Either type y or n + Return : " << std::endl;
                  std::cin.get(upgrade);
                }

                if (std::tolower(upgrade) == 'y') {
                  
                  auto searched = compatible_asset->name + ":";
                  auto sha1info =
                    latest.body.substr(latest.body.find(searched) + searched.size() - 1, 42);

                  std::regex rx_sha1_asset(":(([0-9]|[A-Z]){40})");
                  std::smatch what;
                  std::regex_match(sha1info, what, rx_sha1_asset);
                  auto expected_sha1 = what[1].str();
                  to_lower(expected_sha1);
                  _log << "expected sha1 : " << expected_sha1 << std::endl;

                  try {
                    auto dl_dest = temp_dir() / "downloads";
                    auto final_dest = temp_dir() / std::string(remote);
                    auto upgraded_app = final_dest / _app_path.filename();

                    fs::create_directories(dl_dest);
                    shipxx::ship(
                      remote, 
                      compatible_asset->browser_download_url,
                      expected_sha1, 
                      final_dest,
                      "bin",
                      "Downloading "s + std::string(remote),
                      dl_dest
                    ); 
                    
                    auto system_shell = bp::shell();

                    _log << "We will close, the next time you will come back you will be in the newer vesion" << std::endl;
  
                    #if BOOST_OS_WINDOWS
                    auto str_cmd = "timeout /t 3 /nobreak & del /F /Q "s + _app_path.generic_string() + " & "
                      + "rename " + upgraded_app.generic_string() + " " + _app_path.generic_string();

                    bp::spawn(system_shell, "/c", str_cmd.data());

                    #else 
                    auto str_cmd = "sleep 3; rm "s + _app_path.generic_string() + ";"
                      + "mv " + upgraded_app.generic_string() + " " + _app_path.generic_string() + ";";

                    bp::spawn(system_shell, "-c", str_cmd.data());

                    #endif

                    std::exit(0); // we will never come back
                    
                  } catch (...) {
                    // fail (user message from function in try)
                    // and continue to run into current app
                  }
                } else {
                  std::cout << "You will be reminded in 24h for the update again. Or pass --force-update if you want it earlier" << std::endl;
                }
              }
            } else {
              _log << "👍 You have the latest version : " 
               << "local : " << std::string(current) << ", "
               << "remote : " << std::string(remote) << std::endl;
            
            }

          });
        }
      } catch (...) {
        // Silently fail, if we cannot get the latest update info.
      }
    }

    private:
    std::string github_owner_;
    std::string github_repo_;
    version_t current;
    fs::path _app_path;
    std::ostream& _log;

  };

}
