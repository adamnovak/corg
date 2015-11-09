# Corg: Core Graph Construction Tool

This repository contains a program (called `corg`) which can be used to merge together two VG graphs. The graphs are merged together along correspondingly-named paths (although the paths are not currently preserved in the final graph).

## Installation

To build the tool, make sure you have the VG build dependencies (including protoc and jansson). Then:

```
git clone --recursive https://github.com/adamnovak/corg.git
cd corg
make
```

This will produce a binary, `corg`, in the current directory.

## Usage

Here is a usage example (using sg2vg, vg, and dot):

```
# Obtain some VG graphs with corresponding paths. Preprocess them to have sequential IDs and nodes of reasonable size.
sg2vg "http://ec2-54-149-188-244.us-west-2.compute.amazonaws.com/cactus-brca2/v0.6.g" -u | vg view -Jv - | vg mod -X 100 - | vg ids -s - > "cactus-brca2.vg"
    
sg2vg "http://ec2-54-149-188-244.us-west-2.compute.amazonaws.com/camel-brca2/v0.6.g" -u | vg view -Jv - | vg mod -X 100 - | vg ids -s - > "camel-brca2.vg"

# Before going any further, make sure the input graphs are good
vg validate cactus-brca2.vg
vg validate camel-brca2.vg

# Draw some pictures of the input graphs
vg view -d cactus-brca2.vg > cactus-brca2.dot
dot -Tsvg -o cactus-brca2.svg cactus-brca2.dot
vg view -d camel-brca2.vg > camel-brca2.dot
dot -Tsvg -o camel-brca2.svg camel-brca2.dot

# Compute the core graph. Postprocess it to have nodes of reasonable size.
./corg cactus-brca2.vg camel-brca2.vg | vg mod -uX 100 - | vg ids -s - > core.vg
vg view -d core.vg > core.dot
dot -Tsvg -o core.svg core.dot

# Now admire your core graph in core.svg
```

Note that you need a version of VG with commit 7e195870e7e152a in order for vg
to modify graphs containing mappings on the reverse strand (like the GA4GH bake-off BRCA1).
