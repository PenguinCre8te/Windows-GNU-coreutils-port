#!/usr/bin/env bash
set -euo pipefail

# Build all subdirectories with Makefiles
build_subdirs() {
  for dir in */ ; do
    if [[ -f "$dir/Makefile" ]]; then
      echo "Building in $dir"
      (cd "$dir" && make CC=clang CXX=clang++)
    fi
  done
}

# Package build outputs
package_artifacts() {
  mkdir -p release
  for dir in */ ; do
    if [[ -d "$dir/build" ]]; then
      echo "Packaging $dir/build"
      if [[ "$RUNNER_OS" == "Windows" ]]; then
        powershell Compress-Archive -Path "$dir/build" -DestinationPath "release/${dir%/}.zip"
      else
        tar -czf "release/${dir%/}.tar.gz" -C "$dir" build
      fi
    fi
  done
}

main() {
  build_subdirs
  package_artifacts
}

main "$@"