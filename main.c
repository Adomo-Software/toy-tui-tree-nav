// gcc main.c -lncurses

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <dirent.h>
#include <setjmp.h>
#include <unistd.h>

// #define MAX_LINES LINES-1
#define MAX_LINES LINES

typedef struct Node {
  char *full_name;
	char *name;
  int is_dir;
  int expanded;
  struct Node **children;
  int child_count;
} Node;

Node* create_node(char *name, char* full_name, int is_dir) {
  Node *node = malloc(sizeof(Node));
	node->full_name = strdup(full_name);
  node->name = strdup(name);
  node->is_dir = is_dir;
  node->expanded = 0;
  node->children = NULL;
  node->child_count = 0;
  return node;
}

void add_child(Node *parent, Node *child) {
  parent->children = realloc(parent->children, (parent->child_count + 1) * sizeof(Node *));
  parent->children[parent->child_count++] = child;
}

Node* _build_tree(char *name) {
  DIR *dir;
  struct dirent *entry;
  Node *node = create_node(name, name, 1);

  if (!(dir = opendir(name))) return NULL;

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);

    if (entry->d_type == DT_DIR) {
      Node *child = _build_tree(path);
      if (child) add_child(node, child);
    } else {
      add_child(node, create_node(entry->d_name, path, 0));
    }
  }
  closedir(dir);
  return node;
}

Node* build_tree(char *name) {
	size_t len = strlen(name) - 1;
	if (name[len] == '/') name[len] = '\0';
	_build_tree(name);
}

void print_tree(Node *node, int indent, int *line, int selected_line, int *top_line) {
  if (*line >= *top_line && *line < *top_line + MAX_LINES) {
    if (*line == selected_line) {
			attron(A_REVERSE);
			mvprintw(*line - *top_line, indent, node->name);
			attroff(A_REVERSE);
		} else {
			mvprintw(*line - *top_line, indent, node->name);
		}
  }

  (*line)++;

  if (node->is_dir && node->expanded) {
    for (int i = 0; i < node->child_count; i++) {
      print_tree(node->children[i], indent + 2, line, selected_line, top_line);
    }
  }
}

jmp_buf jumpBuffer;

void toggle_expand_or_return_path(Node *node, int *line, int target_line, int *top_line, char *name) {
	if (*line >= *top_line && *line < *top_line + MAX_LINES) {
    if (*line == target_line) {
			if (node->is_dir) {
				node->expanded = !node->expanded;
				longjmp(jumpBuffer, 1);
			}
			else {
				endwin();
				printf("%s\n", node->full_name);
				exit(0);
			}
		}
  }

  (*line)++;

  if (node->is_dir && node->expanded) {
    for (int i = 0; i < node->child_count; i++) {
      toggle_expand_or_return_path(node->children[i], line, target_line, top_line, name);
    }
  }
}

int main(int argc, char **argv) {

  char *path = (argc > 1) ? argv[1] : ".";
  Node *root = build_tree(path);
	
  FILE* out = NULL;
	SCREEN* screen = NULL;
  if (!isatty(fileno(stdout))) {
		// this is some terminal bs to make us able to use pipes & command substitution (idk)
		// https://stackoverflow.com/a/17453746/11688800
    out = fopen("/dev/tty", "r+");
    setbuf(out, NULL);
		screen = newterm(NULL, out, out);
		set_term(screen);
	} else {
		initscr();
	}

  noecho();
  cbreak();
  keypad(stdscr, TRUE);
	curs_set(0);

	int selected_line = 0;
	int ch;
  
  int top_line = 0;

  int line = 0;
	int prev_line = 1;
  print_tree(root, 0, &line, selected_line, &top_line);
  refresh();

  while ((ch = getch()) != 'q') {
    line = 0;

    switch (ch) {
    case KEY_UP:
      if (selected_line > 0) selected_line--;
			if (selected_line < top_line) top_line--;
      break;
		case KEY_DOWN:
			if (selected_line <= top_line) top_line = selected_line;
			if (selected_line < prev_line - 1) selected_line++;
      if (selected_line >= MAX_LINES + top_line) top_line++;
      break;
    case '\n':
      line = 0;
			if (setjmp(jumpBuffer) == 0) {
				toggle_expand_or_return_path(root, &line, selected_line, &top_line, path);
			}
			
      break;
		}

    werase(stdscr);
    line = 0;
		
    print_tree(root, 0, &line, selected_line, &top_line);
		// mvprintw(MAX_LINES, 0, "line: %d selected_line: %d %d %d", line, selected_line, top_line, MAX_LINES);
		prev_line = line;

    refresh();
  }

	endwin();
	delscreen(screen);
	
  return 0;
}
