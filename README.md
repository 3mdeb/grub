# GRUB2

[![pipeline status](https://gitlab.com/trenchboot1/3mdeb/grub/badges/trenchboot_support_2.04/pipeline.svg)](https://gitlab.com/trenchboot1/3mdeb/grub/-/commits/trenchboot_support_2.04)

This is GRUB 2, the second version of the GRand Unified Bootloader.
GRUB 2 is rewritten from scratch to make GNU GRUB cleaner, safer, more
robust, more powerful, and more portable.

On the TrenchBoot purposes, GRUB 2 uses the slaunch module to enable secure
launch.

## News

See the file [NEWS](NEWS) for a description of recent changes to GRUB 2.

## Instalation

See the file [INSTALL](INSTALL) for instructions on how to build and install the
GRUB 2 data and program files.

## Continuous Integration

- testing basic building
- testing slaunch module presence
- testing NixOS pacakge building
- testing debian package building

## More information

Please visit the official web page of GRUB 2, for more information.
The URL is <http://www.gnu.org/software/grub/grub.html>.

More extensive documentation is available in the Info manual,
accessible using 'info grub' after building and installing GRUB 2.

There are a number of important user-visible differences from the
first version of GRUB, now known as GRUB Legacy. For a summary, please
see:

  info grub Introduction 'Changes from GRUB Legacy'
