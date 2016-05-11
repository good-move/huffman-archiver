#include <stdio.h>
#include <stdlib.h>

#define PROD
typedef unsigned int UINT32;
typedef unsigned long long int UINT64;

typedef struct tree_s {
  struct tree_s* parent;
  struct tree_s* left;
  struct tree_s* right;
  unsigned char symbol;
  unsigned int weight;
} Tree;

typedef struct buffer_s
{
  unsigned int last_bit; // points at the position to write a bit to
  unsigned int end_bit;
  unsigned char buffer[1024];
} Buffer;

/* ************************************ HELPER FUNCTIONS ************************************ */

void print_file_info(const Tree* array[]) // prints to "file_info.txt"
{
  FILE* file = fopen("file_info.txt", "w");
  for (int i = 0; i < 256; i++)
    if (array[i] != NULL)
      fprintf(file, "Symbol: %c; Repetitions: %u; ASCII code: %d;\n", array[i]->symbol, array[i]->weight, array[i]->symbol);
  fclose(file);
}

void print_tree_(Tree* tree, FILE* file)
{
  if (tree == NULL) {
    fprintf(file, "null");
    return;
  }
  fprintf(file, "%c (", tree->symbol);
  print_tree_(tree->left, file);
  fprintf(file, ", ");
  print_tree_(tree->right, file);
  fprintf(file, ")");
}

void print_tree(Tree* tree, const char* file_name)
{
  FILE* file = fopen(file_name, "w");
  print_tree_(tree, file);
  fclose(file);
}

void print_code_array(const UINT64 array[][2]) // prints to "codes.txt"
{
  FILE* file = fopen("codes.txt", "w");
  for (int i = 0; i < 256; i++)
  {
    if (array[i][1] > 0) {
      fprintf(file, "%c: ", i);
      for (int j = array[i][1] - 1; j >= 0; j--)
        fprintf(file, "%lld", (array[i][0] >> j) & 1);
      fprintf(file, "\n");
    }
  }
  fclose(file);
}

/* ************************************ WORKFLOW FUNCTIONS ************************************ */

Tree* add_parent(Tree* parent, Tree* left_node, Tree* right_node)
{
  if (parent == NULL) parent = (Tree*) malloc(sizeof(Tree));
  parent->left = left_node;
  parent->right = right_node;
  parent->symbol = '#';
  if (left_node && right_node != NULL)
    parent->weight = left_node->weight + right_node->weight;
  else
    parent->weight = 0;
  if (left_node != NULL)
    left_node->parent = parent;
  if (right_node != NULL)
    right_node->parent = parent;
  return parent;
}

Tree* get_tree_node(const unsigned char c)
{
  Tree* tree = (Tree*)malloc(sizeof(Tree));
  tree->symbol = c;
  tree->weight = 1;
  tree->parent = NULL;
  tree->left = NULL;
  tree->right = NULL;

  return tree;
}

void destroy_tree(Tree* tree)
{
  if (tree == NULL) return;
  if (tree->left == NULL && tree->right == NULL) {
    free(tree);
    tree = NULL;
    return;
  }
  destroy_tree(tree->left);
  destroy_tree(tree->right);
  free(tree);
  tree = NULL;
}

int compare_frequency(const void* a, const void* b)
{
  if (*(Tree**)a == NULL) return 1;
  if (*(Tree**)b == NULL) return -1;

  int l = (*((Tree**)a))->weight;
  int r = (*((Tree**)b))->weight;

  if (l > r) return -1;
  if (l < r) return 1;
  return 0;
}

void fill_buffer(FILE* file, Buffer* buffer)
{
  memset(buffer->buffer, 0, 1024);
  int bytes_read = fread(buffer->buffer, sizeof(buffer->buffer[0]), 1024, file);
  buffer->last_bit = 0;
  buffer->end_bit = 8 * bytes_read - 1;
}

void flush_buffer(FILE* file, Buffer* buffer)
{
  fwrite(buffer->buffer, sizeof((buffer->buffer)[0]), buffer->last_bit / 8, file);
  memset(buffer->buffer, 0, 1024);
  buffer->last_bit = 0;
}

void add_bit_to_buffer(FILE* file, Buffer* buffer, unsigned char bit)
{
  if (buffer->last_bit == 1024*8) flush_buffer(file, buffer);
  buffer->buffer[(buffer->last_bit) / 8] |= (bit << ( 7 - (buffer->last_bit % 8)));
  buffer->last_bit++;
}

void add_code_to_buffer(FILE* file, Buffer* buffer, UINT64 code, UINT64 length)
{
  for (int i = length - 1; i >= 0; i--) {
    unsigned char bit = (code >> i) & 1;
    add_bit_to_buffer(file, buffer, bit);
  }
}

void add_char_to_buffer(FILE* file, Buffer* buffer, unsigned char symbol)
{
  for (int i = 7; i >=0; i--) {
    unsigned char bit = (symbol >> i) & 1;
    add_bit_to_buffer(file, buffer, bit);
  }
}

unsigned char read_bit_from_buffer(FILE* file, Buffer* buffer)
{
  if (buffer->last_bit == 1024*8) fill_buffer(file, buffer);

  unsigned char c = ((buffer->buffer[buffer->last_bit / 8] >> (7 - (buffer->last_bit % 8))) & 1);
  buffer->last_bit++;
  return c;
}

unsigned char read_bits_from_buffer(FILE* file, Buffer* buffer, const UINT32 n)
{
  unsigned char result = 0;
  for (int i = n-1; i >= 0; i--)
    result |= read_bit_from_buffer(file, buffer) << i;
  return result;
}

void build_code_tree(Tree* tree_array[])
{
  int counter = -1;
  int i = 0;
  while(i < 256 && tree_array[i] != NULL) (i++, counter++);

  if (counter == -1) return;

  while (counter > 0) {
    tree_array[counter-1] = add_parent(NULL, tree_array[counter-1], tree_array[counter]);
    qsort(tree_array, counter, sizeof(tree_array[0]), compare_frequency);
    counter--;
  }
}

void build_code_array(Tree* code_tree, UINT64 code_array[][2], UINT64 code, int length)
{
  if (code_tree == NULL) return;
  if (code_tree->left == NULL) {
    code_array[code_tree->symbol][0] = code;
    code_array[code_tree->symbol][1] = (length == 0 ? 1 : length);
    return;
  }
  build_code_array(code_tree->left, code_array, code<<1, length+1);
  build_code_array(code_tree->right, code_array, (code<<1)|1, length+1);
}

int get_est_code_length(Tree* code_tree, UINT64 code_array[][2])
{
  if (code_tree == NULL) return 0;
  if (code_tree->left == NULL) {
    return (1 + code_tree->weight * code_array[code_tree->symbol][1]) % 8;
  }

  return 1 +
          get_est_code_length(code_tree->left, code_array) +
          get_est_code_length(code_tree->right, code_array);
}

void write_extra_bits(Buffer* buffer, int extra_bits)
{
  for (int i = 0; i < 3; i++) {
    unsigned char bit = (extra_bits >> (2 - i)) & 1;
    add_bit_to_buffer(NULL, buffer, bit);
  }
}

void write_code_tree(FILE* file, Tree* code_tree, Buffer* buffer)
{
  if (code_tree == NULL) return;
  if (code_tree->left == NULL) {
    add_bit_to_buffer(file, buffer, 1);
    add_char_to_buffer(file, buffer, code_tree->symbol);
  }
  else {
    add_bit_to_buffer(file, buffer, 0);
  }
  write_code_tree(file, code_tree->left, buffer);
  write_code_tree(file, code_tree->right, buffer);
}

Tree* restore_code_tree(FILE* file, Tree* code_tree, Buffer* buffer)
{
  while(1) {
    unsigned char c = read_bits_from_buffer(file, buffer, 1);
    if (!code_tree) code_tree = get_tree_node('#');
    else {
        code_tree = add_parent(
                  code_tree,
                  !(code_tree->left) ? get_tree_node('#') : code_tree->left,
                  !(code_tree->left) ? NULL : get_tree_node('#')
                );
        code_tree = (!(code_tree->right) ? code_tree->left : code_tree->right);
    }
    if (c == 1) {
      code_tree->symbol = read_bits_from_buffer(file, buffer, 8);
      while (1) { // climb up the tree until there's a place to insert new child node
        if (code_tree->parent == NULL) break; // in case the tree consists of only one node — a root
        code_tree = code_tree->parent;
        if (code_tree->right == NULL || code_tree->parent == NULL) break;
      }
      if (code_tree->right != NULL && code_tree->parent == NULL || code_tree->left == NULL) break; // tree is built
    }
  }
  return code_tree;
}

void decode_text(FILE* file, const char* output_file_name, Tree* code_tree, Buffer* buffer)
{
  FILE* output_file = fopen(output_file_name, "wb");

  while(1) {
    Tree* walker = code_tree;
    while(1) {
      unsigned char c = read_bits_from_buffer(file, buffer, 1);
      if (walker->left == NULL) break; // in case it's a one-noded tree
      if (c == 0) walker = walker->left;
      if (c == 1) walker = walker->right;
      if (walker->left == NULL) break; // leave reached
    }
    fputc(walker->symbol, output_file);

    if (feof(file) !=0 && buffer->last_bit > buffer->end_bit) break;
  }
  fclose(output_file);
}

void encode(FILE* input_file, const char* output_file_name)
{

  UINT64 code_array[256][2] = {{0ULL}}; // [[code, code length], ...]
  Tree* alphabet[256] = {0};
  Buffer buffer = {0, 0, ""};
  memset(buffer.buffer, 0, 1024);
  unsigned char c;      // c: char to read from an input file when encoding

  while(1) {
    c = fgetc(input_file);
    if (feof(input_file)) break;

    if (alphabet[c] == NULL) alphabet[c] = get_tree_node(c);
    else (alphabet[c]->weight)++;
  }

  qsort(alphabet, 256, sizeof(alphabet[0]), compare_frequency);

  build_code_tree(alphabet);
  build_code_array(alphabet[0], code_array, 1ULL, 0);

  // DEBUG INFO (is written into auxiliary files)
  print_file_info(alphabet);
  print_tree(alphabet[0], "encoded_tree.txt");
  print_code_array(code_array);
  //

  int extra_bits = (8 - (3 + get_est_code_length(alphabet[0], code_array)) % 8) % 8;

  write_extra_bits(&buffer, extra_bits);
  buffer.last_bit = 3 + extra_bits;

  FILE* output_file = fopen(output_file_name, "wb");
  write_code_tree(output_file, alphabet[0], &buffer);

  fseek(input_file, 3, SEEK_SET);

  while(1) {
    c = fgetc(input_file);
    if (feof(input_file) != 0) break;

    add_code_to_buffer(output_file, &buffer, code_array[c][0], code_array[c][1]);
  }
  flush_buffer(output_file, &buffer);
  destroy_tree(alphabet[0]);
  fclose(output_file);
}

void decode(FILE* input_file, const char* output_file_name)
{
  Tree* code_tree  = NULL;
  Buffer buffer = {0, 0, ""};

  fill_buffer(input_file, &buffer);
  unsigned char extra_bits = read_bits_from_buffer(input_file, &buffer, 3);
  buffer.last_bit = 3 + extra_bits;

  code_tree = restore_code_tree(input_file, code_tree, &buffer);
  decode_text(input_file, output_file_name, code_tree, &buffer);

  // DEBUG INFO
  print_tree(code_tree, "decoded_tree.txt");

  destroy_tree(code_tree);
}

int main()
{
  char MODE;  // MODE: archiver work mode
  FILE* input_file;

#ifdef PROD
  input_file = fopen("in.txt", "rb");
#else
  input_file = fopen("input.txt", "rb");
#endif

  MODE = fgetc(input_file);
  fgetc(input_file); // read \r
  fgetc(input_file); // read \n
  fgetc(input_file); // check next char

  if (feof(input_file) != 0)
  {
    fclose(input_file);
    return 0;
  }
  fseek(input_file, 3, SEEK_SET);

#ifdef PROD
  if (MODE == 'c') encode(input_file, "out.txt");
  if (MODE == 'd') decode(input_file, "out.txt");
#else
  if (MODE == 'c') encode(input_file, "encoded.txt");
  if (MODE == 'd') decode(input_file, "decoded.txt");
#endif


  fclose(input_file);

  return 0;
}