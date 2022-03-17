#include <iostream>

#include <upgrd/upgrd.hxx>

int main(int argc, const char *argv[]) {
  argc=argc+1;
  argv[1]="--force-upgrade";
  upgrd::manager up{"nxxm", "example-upgrd-app", "v0.0.1", argc, argv, std::cout};
  up.propose_upgrade_when_needed();

  std::cout << "Welcome to small-app version " << up.current_version() << std::endl;

  return 0;
}
