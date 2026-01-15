##  How to Compile

1. Go to the directory **`HW2/src/`**.

2. Run the following command to compile the project:

   ```bash
   $ make
   ```

   After compilation, the executable file **`hw2`** will be generated in **`HW2/bin/`**.

3. To remove the executable and all intermediate files, use:

   ```bash
   $ make clean
   ```

---

##  How to Run

### Usage:

```bash
./hw2 <input file> <output file> <number of partitions>
```

### Example:

When you are in the **`HW2/bin/`** directory, run:

```bash
$ ./hw2 ../testcase/public1.txt ../output/public1.2way.out 2
```

