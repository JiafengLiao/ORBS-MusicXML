# ORBS-MusicXML
A musicXML slicer written in C running with scripts 
contributed on libxml++(http://libxmlplusplus.sourceforge.net/), musly(http://www.musly.org/) 
timidity++ (http://timidity.sourceforge.net/#info)

-------------

This project is about exploring the music compression in symbolic perspective by building an application of MusicXML file slicer using tree observational algorithm. 


Requisition (library installation):
1. libxml++(2.6 or 3)
This is for parsing XML file to tree-node structure
2. glib
3. glibmm++
4. boost
5. c++11 compiler
6. Musly
Musly is a fast and high quality audio similarity library.
7. timidity++
For MIDI file conversion

MacOS & Linux:

Compilation:
For libxml++3
Enter the compile command in command tool line(-I are for the directory search for header files)
The path of directory may vary due to installation method: (home-brew suggested)

g++ main.cpp -I/usr/local/include -I/usr/local/Cellar/libxml++3/3.0.1/include/libxml++-3.0  -I/usr/local/Cellar/libxml++3/3.0.1/include/libxml++-3.0/libxml++  -I/usr/local/Cellar/glib/2.52.0/include/glib-2.0 -I/usr/local/Cellar/glibmm/2.50.1/include/glibmm-2.4 -I/usr/local/lib/glib-2.0/include -I/usr/local/lib/glibmm-2.4/include -I/usr/local/lib/libxml++-3.0/include -std=c++11 -lmusly -lboost_filesystem -lboost_system -lxml++-3.0 -lglibmm-2.4 -w -o main

For libxml++2.6
g++ main.cpp -I/usr/local/include -I/usr/local/Cellar/libxml++/2.40.1/include/libxml++-2.6 -I/usr/local/Cellar/glib/2.52.0/include/glib-2.0 -I/usr/local/Cellar/glibmm/2.50.1/include/glibmm-2.4 -I/usr/local/lib/glib-2.0/include -I/usr/local/lib/glibmm-2.4/include -I/usr/local/lib/libxml++-2.6/include -std=c++11 -lmusly -lboost_filesystem -lboost_system -lxml++-2.6 -lglibmm-2.4 -w -o main

Run:

simply run by ./main path threshold
For calculating runtime, run by :  usr/bin/time ./main path threshold
Example:
usr/bin/time ./Example1.xml 1

The path is path the Music.xml and threshold should be positive double number. (0.01, 10 for example)

Examples:
The testing result of 2 examples are in example_test_results
The example MusicXML are provided by http://www.musicxml.com/music-in-musicxml/example-set/
more examples can be downloaded freely in this website.
