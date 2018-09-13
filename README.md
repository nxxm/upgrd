# upgrd library : upgrade support for nxxm powered apps

[nxxm](https://nxxm.github.io/) is a modern dependency managers that goes beyond dependency management but also performs upgrade.

This is the upgrd library, with the ecosystem that **nxxm** is providing the goal is to make it a complete toolkit to perform software upgrade.

## Features
  - Relies on GitHub releases : once you tag and validate the release the apps starts downloading by your end user.
  - Provides a simple to one-liner API for the usual scenarii. 

```cpp

  upgrd::manager up{
    "nxxm",
    "example-upgrd-app",
    "v0.0.1",
    argc,
    argv,
    std::cout
  };

  up.propose_upgrade_when_needed();


```

This will add the following command line switches to your app : 
  * `--dont-upgrade` : never perform the up-to-date check
  * `--force-upgrade` : force program upgrade (always check)
  * `--auto-upgrade` : check normally (once a day) but always do the upgrade without asking user.
