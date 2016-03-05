# Greetings
Welcome! Thank you for considering the README file.

**Contents**
  1. What is mkgraph?
  2. What is the project status?
  3. Installation
  4. Using
  5. Credits
  6. Contact

# 1 What is mkgraph?
It is a subproject of OpenTTD to export **route networks**, i.e. graphs that
depict the tracks and stations of your OpenTTD map.

The project consists of two parts:
  * The **graph driver**: This is a pseudo-video driver in OpenTTD which dumps
    a binary file containing all the stations, their connections and other
    data like carried cargo. It uses the YAPF pathfinder.
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
git clone https://github.com/JohannesLorenz/OpenTTD openttd-mkgraph
cd openttd-mkgraph
# now, follow the usual
# [installation instructions for OpenTTD](../../readme.txt)
# then, type
clang++ -lm -g -ggdb -Wall -Wextra -std=c++11 -DEXPORTER src/mkgraph/*.cpp \
  -o mkgraph
```
You can also use g++ instead of clang++.

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
  -s null -m null | ./mkgraph.exe | dot -Kneato -Tpdf
```

# 5 Credits
The author thanks:
  * The many helpful people in the OpenTTD chats/forums for helpful
    their patience and their helpful suggestions

# 6 Contact

Feel free to give feedback. The author's e-mail address is shown if you \
execute this in a shell:
```sh
printf "\x6a\x6f\x68\x61\x6e\x6e\x65\x73\x40\x6c\x6f\
\x72\x65\x6e\x7a\x2d\x68\x6f\x2e\x6d\x65\x0a"
```


