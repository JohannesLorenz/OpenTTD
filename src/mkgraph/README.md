# Greetings
Welcome! Thank you for considering the README file.

**Contents**
  1. What is mkgraph?
  2. What is the project status?
  3. Documentation
  4. Credits
  5. Contact
  6. Enclosed documents

# 1 What is mkgraph?
It is a subproject of openttd to export **route networks**, i.e. graphs that
depict the tracks and stations of your railway.

The project consists of two parts:
  * The **graph driver**: This is a pseudo-video driver in OpenTTD which dumps
    a datafile containing all the stations, their connections and other data
    like carried cargo into a binary file. It uses the YAPF pathfinder.
  * The **mkgraph** utility: It takes the binary file and converts it into a
    *.dot* file (an input file for tools that generate PDFs, see the "Using"
    section). Contrary to the graph driver, the mkgraph utility can take
    parameters for customization.

# 2 What is the project status?
It is currently a testing-only version. The code does not yet follow any
coding guidelines and has not been approved by the OpenTTD developers.

# 3 Installation
## Requirements
  * A C++11 compiler
  * the dot utility (called "dot" on linux)

## Installing
```sh
make install # probably to some non-system path
clang++ -lm -g -ggdb -Wall -Wextra -std=c++11 -DEXPORTER src/mkgraph/*.cpp \
  -o mkgraph
```

# 4 Using
If you want to do it step by step:
```sh
/where/you/installed/openttd -g ~/.openttd/save/your_savegame.sav -v graph \
  -s null -m null > graph.dat 2>/dev/null
cat graph.dat | ./mkgraph > graph.dot 2>/dev/null
cat graph.dot | dot -Kneato -Tpdf > graph.pdf
```
Otherwise, you can also use pipes:
```sh
/where/you/installed/openttd -g ~/.openttd/save/your_savegame.sav -v graph \
  -s null -m null > graph.dat 2>/dev/null | ./mkgraph.exe | dot -Kneato -Tpdf
```

# 4 Credits
The author thanks:
  * The many helpful people in the OpenTTD chats/forums for helpful
    their patience and their helpful suggestions

# 5 Contact

Feel free to give feedback. The author's e-mail address is shown if you \
execute this in a shell:
```sh
printf "\x6a\x6f\x68\x61\x6e\x6e\x65\x73\x40\x6c\x6f\
\x72\x65\x6e\x7a\x2d\x68\x6f\x2e\x6d\x65\x0a"
```


