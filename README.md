This program scans Google Flatbuffer binary files and attempts to reverse engineer a schema. Mainly used for reverse engineering Pokemon Legends ZA files.
If there is a compiled binary available, it will be found in the released tab of this repository.

How to use:
```flatminer.exe path-to-file.bin```

Optional Arguments
```
-ao : Does not create schema files, only print analysis of root table and its direct sub tables.
```

What it will do:
A "schemas" folder will be created wherever you run the program. The initial schema will be named <name_of_input_file>.fbs
There, generated nested schema files will be created with names based on the member index of its parent table. For example,
if table `a.fbs` has a nested table at member "unk2", then the resulting table will be called `a_2.fbs`. If `a_2.fbs` has
a nested table at member "unk4", then the resulting table will be called `a_2_4.fbs`. However, if the member above the
nested table is a string, it will automatically assume that the string is the name of the nested table. For example,
if `a_2_4.fbs` has a nested table at index 2, but index 1 is a string that reads "trinity_gluePlugin", then
the resulting schema file will be called `trinity_gluePlugin.fbs`.

What it does not support:
- union types
- schema linking for array of tables with different schemas (The resulting FBS will simply use whatever is at index 0)





