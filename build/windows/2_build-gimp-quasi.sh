#!/bin/sh

set -e


# SHELL ENV
if [ -z "$QUASI_MSYS2_ROOT" ]; then

if [ -z "$GITLAB_CI" ]; then
  # Make the script work locally
  if [ "$0" != 'build/windows/2_build-gimp-quasi.sh' ] && [ ${PWD/*\//} != 'windows' ]; then
    echo -e '\033[31m(ERROR)\033[0m: Script called from wrong dir. Please, read: https://developer.gimp.org/core/setup/build/windows/'
    exit 1
  elif [ ${PWD/*\//} = 'windows' ]; then
    cd ../..
  fi

  git submodule update --init

  PARENT_DIR='../'
fi


# FIXME: We need native/Linux gimp-console.
# https://gitlab.gnome.org/GNOME/gimp/-/issues/6393
if [ "$1" ]; then
  export BUILD_DIR="$1"
else
  export BUILD_DIR=$(echo _build*$RUNNER)
fi
if [ ! -d "$BUILD_DIR" ]; then
  echo -e "\033[31m(ERROR)\033[0m: Before running this script, first build GIMP natively in $BUILD_DIR"
fi
if [ -z "$GITLAB_CI" ] && [ -z "$GIMP_PREFIX" ]; then
  export GIMP_PREFIX="$PWD/../_install"
fi
if [ ! -d "$GIMP_PREFIX" ]; then
  echo -e "\033[31m(ERROR)\033[0m: Before running this script, first install GIMP natively in $GIMP_PREFIX"
fi
if [ ! -d "$BUILD_DIR" ] || [ ! -d "$GIMP_PREFIX" ]; then
  echo 'Patches are very welcome: https://gitlab.gnome.org/GNOME/gimp/-/issues/6393'
  exit 1
fi
GIMP_CONSOLE_PATH=$PWD/${PARENT_DIR}quasi-msys2/root/clang64/bin/gimp-console
echo "#!/bin/sh" > $GIMP_CONSOLE_PATH
IFS=$'\n' VAR_ARRAY=($(cat .gitlab-ci.yml | sed -n '/export PATH=/,/GI_TYPELIB_PATH}\"/p' | sed 's/    - //'))
IFS=$' \t\n'
for VAR in "${VAR_ARRAY[@]}"; do
  echo $VAR >> $GIMP_CONSOLE_PATH
done
echo "$GIMP_PREFIX/bin/gimp-console \"\$@\"" >> $GIMP_CONSOLE_PATH
chmod u+x $GIMP_CONSOLE_PATH

if [ "$GITLAB_CI" ]; then
  # Install crossroad deps again
  # We take code from deps script to better maintenance
  echo "$(cat build/windows/1_build-deps-quasi.sh                 |
          sed -n '/# Beginning of install/,/# End of install/p')" | bash
fi

## The required packages for GIMP are taken from the result of previous script


# QUASI-MSYS2 ENV
export CC='/usr/bin/clang'
export CXX='/usr/bin/clang++'
export QUASI_MSYS2_FAKEBIN_BLACKLIST='python3'
sudo ln -nfs "$PWD/quasi-msys2/root/clang64" /clang64
bash -c "source quasi-msys2/env/all.src && bash build/windows/2_build-gimp-quasi.sh"
else

export ARTIFACTS_SUFFIX="-cross"
export GIMP_PREFIX="$PWD/_install${ARTIFACTS_SUFFIX}"

sed -i "s|'windres'|\'llvm-windres-19\'|g" quasi-msys2/env/config/meson_cross_file.ini

## Build GIMP
if [ ! -f "_build$ARTIFACTS_SUFFIX/build.ninja" ]; then
  meson setup _build$ARTIFACTS_SUFFIX -Dprefix="$GIMP_PREFIX" -Dgi-docgen=disabled \
                                      -Djavascript=disabled -Dvala=disabled
fi
cd _build$ARTIFACTS_SUFFIX
ninja
ninja install
ccache --show-stats
cd ..
fi # END OF QUASI-MSYS2 ENV
