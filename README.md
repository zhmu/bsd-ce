# BSD-CE

This is a small utility which can convert a FreeBSD kernel ELF files to Windows CE `NK.BIN` files. These files can then be loaded by the Windows CE bootloader and you'll have FreeBSD on your previously CE-only device!

## Usage

I used this tool to inject a kernel in a Compaq Evo T30 thinclient; once booting the machine and holding _P_, it will attempt to do an netboot using DHCP/TFTP on ports 10067, 10068 and 10069. Grabbing the original firmware and just overwriting the chunk that began with `NK` (which is the magic Windows CE kernel signature) allowed me to place my own firmware in there - and it turns out FreeBSD works mostly fine on such a device.

Mostly - because there seem to be some (hardware?) issues that FreeBSD cannot cope with:

  * I was unable to get the onboard IDE controller to do anything elseful (it would never find any disks)
  * The USB chip used has a serious hardware defect, which neither the pre-8.0 nor the new post-8.0 USB stack have workarounds for
  * The PCMCIA interface does not seem to have an IRQ line, resulting in no devices attaching to it

 On the bright side, the audio interface works fine, as does ethernet (on Linux, only 2.4 kernels would work correctly with it).

# Downloads

  * [15 March 2008 version](releases/bsd-ce-20080315.tar.bz2) (6KB)
