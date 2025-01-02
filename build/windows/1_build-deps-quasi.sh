#!/bin/sh

set -e


# SHELL ENV
if [ -z "$QUASI_MSYS2_ROOT" ]; then

if [ -z "$GITLAB_CI" ]; then
  # Make the script work locally
  if [ "$0" != 'build/windows/1_build-deps-quasi.sh' ] && [ ${PWD/*\//} != 'windows' ]; then
    echo -e '\033[31m(ERROR)\033[0m: Script called from wrong dir. Please, read: https://developer.gimp.org/core/setup/build/windows/'
    exit 1
  elif [ ${PWD/*\//} = 'windows' ]; then
    cd ../..
  fi

  export GIT_DEPTH=1

  export GIMP_DIR=$(echo "$PWD/")
  cd $(dirname $PWD)
fi


## Install quasi-msys2 and its deps
# Beginning of install code block
if [ "$GITLAB_CI" ]; then
  apt-get update -y
  apt-get install -y --no-install-recommends \
                     git                     \
                     ccache                  \
                     meson                   \
                     clang                   \
                     lld                     \
                     llvm                    \
                     sudo                    \
                     make                    \
                     wget                    \
                     tar                     \
                     zstd                    \
                     gawk                    \
                     gpg
fi
# End of install code block
if [ ! -d 'quasi-msys2' ]; then
  git clone --depth $GIT_DEPTH https://github.com/HolyBlackCat/quasi-msys2
fi
cd quasi-msys2
git pull
cd ..

## Install the required (pre-built) packages for babl, GEGL and GIMP
echo -e "\e[0Ksection_start:`date +%s`:deps_install[collapsed=true]\r\e[0KInstalling dependencies provided by MSYS2"
echo CLANG64 > quasi-msys2/msystem.txt
deps=$(cat ${GIMP_DIR}build/windows/all-deps-uni.txt |
       sed "s/\${MINGW_PACKAGE_PREFIX}-/_/g"         | sed 's/\\//g')
cd quasi-msys2
make install _clang $deps || $true
cd ..
echo -e "\e[0Ksection_end:`date +%s`:deps_install\r\e[0K"


# QUASI-MSYS2 ENV
echo -e "\e[0Ksection_start:`date +%s`:cross_environ[collapsed=true]\r\e[0KPreparing build environment"
export CC='/usr/bin/clang'
export CXX='/usr/bin/clang++'
sudo ln -nfs "$PWD/quasi-msys2/root/clang64" /clang64
bash -c "source quasi-msys2/env/all.src && bash ${GIMP_DIR}build/windows/1_build-deps-quasi.sh"
else

## Prepare env (only GIMP_PREFIX var is needed)
export GIMP_PREFIX="/clang64"
echo -e "\e[0Ksection_end:`date +%s`:cross_environ\r\e[0K"


## Build babl and GEGL
self_build ()
{
  echo -e "\e[0Ksection_start:`date +%s`:${1}_build[collapsed=true]\r\e[0KBuilding $1"

  # Clone source only if not already cloned or downloaded
  if [ ! -d "$1" ]; then
    git clone --depth $GIT_DEPTH https://gitlab.gnome.org/gnome/$1
  fi
  cd $1
  git pull

  if [ ! -f "_build-cross/build.ninja" ]; then
    meson setup _build-cross -Dprefix="$GIMP_PREFIX" $2
  fi
  cd _build-cross
  ninja
  ninja install
  ccache --show-stats
  cd ../..
  echo -e "\e[0Ksection_end:`date +%s`:${1}_build\r\e[0K"
}

self_build babl '-Denable-gir=false'
self_build gegl '-Dintrospection=false'
fi # END OF QUASI-MSYS2 ENV
