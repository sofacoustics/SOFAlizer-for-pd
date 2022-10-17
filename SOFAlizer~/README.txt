/* ****************************************************************************************************************** */
/* RT binaural filter: SOFAlizer~                                                                                     */
/* based on earplug~ by Pei Xiang (summer 2004), Jorge Castellanos (Fall 2006), Hans-Christoph Steiner (Spring 2009). */
/* ****************************************************************************************************************** */
/* Author: Christian Klemenschitz                                                                                     */
/* For: Acoustic Research Institute, Wohllebengasse 12-14, A-1040, Vienna, Austria                                    */
/* Contact: Piotr Majdak, piotr@majdak.com                                                                            */
/* ****************************************************************************************************************** */

/* Interactive, binaural real-time syntheses with HRTF sets provided from a SOFA file. */

Configure Makefile at:
    LDFLAGS += -L$(DIR)~/SOFAlizer-for-pd/SOFAlizer~ -Wl,-R$(DIR)~/SOFAlizer-for-pd/SOFAlizer~'-Wl,-R$$ORIGIN'
Change paths to directory of ./SOFAlizer~ 

Run Makefile: make

SOFAlizer~.pd_linux is created.

Copy external SOFAlizer~.pd_linux, SOFAlizer~-help.pd, all *.sofa files and Lament.wav to pd-externals directory.
