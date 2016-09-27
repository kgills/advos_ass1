#define MAX_PATHLEN 	128
#define MAX_N 		 	128

// TODO: make the length dynamic

typedef struct {
	unsigned complete; 			// non-zero for completion message
	unsigned origin; 			// which node the token came from
	int path_i; 				// current location in the path
	int path[MAX_PATHLEN];		// path that the token needs to take
	unsigned sum[MAX_N];		// sum for each node to increment
} token_t;