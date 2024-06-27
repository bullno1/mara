#include "internal.h"
#include "xxhash.h"

MARA_WARNING_PUSH()
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunneeded-internal-declaration"
#endif

// Sync this list with compiler.c
static const char* mara_builtin_symbols[] = {
	"def", "set", "if", "fn", "do", "nil", "true", "false", "import", "export"
};

MARA_WARNING_POP()

static const size_t MARA_NUM_BUILTIN_SYMBOLS =
	sizeof(mara_builtin_symbols) / sizeof(mara_builtin_symbols[0]);

void
mara_symtab_init(mara_env_t* env, mara_symtab_t* symtab) {
	(void)env;
	(void)symtab;
}

mara_index_t
mara_symtab_intern(mara_env_t* env, mara_symtab_t* symtab, mara_str_t string) {
	mara_index_t index_itr = symtab->len > 0 ? 0 : -1;
	uint64_t hash_itr = mara_XXH3_64bits(string.data, string.len);
	mara_index_t last_node_index = index_itr;
	uint64_t last_hash_itr = hash_itr;
	for (; index_itr >= 0; hash_itr >>= BHAMT_NUM_BITS) {
		mara_symtab_node_t* node = &symtab->nodes[index_itr];
		if (mara_str_equal(node->key, string)) {
			break;
		}

		// Offset all indicies by 1 so we can zero the new node
		last_node_index = index_itr;
		last_hash_itr = hash_itr;
		index_itr = node->children[hash_itr & BHAMT_MASK] - 1;
	}

	if (index_itr >= 0) {
		return index_itr;
	} else {
		mara_index_t current_len = symtab->len;
		mara_index_t current_capacity = symtab->capacity;
		if (current_len >= symtab->capacity) {
			mara_index_t new_capacity = current_capacity > 0
				? current_capacity * 2
				: (mara_index_t)MARA_NUM_BUILTIN_SYMBOLS * 2;
			symtab->nodes = mara_realloc(
				env->options.allocator,
				symtab->nodes,
				sizeof(mara_symtab_node_t) * new_capacity
			);
			mara_assert(symtab->nodes != NULL, "Out of memory");
			symtab->capacity = new_capacity;
		}

		mara_symtab_node_t* new_node = &symtab->nodes[symtab->len++];

		memset(new_node->children, 0, sizeof(new_node->children));
		char* chars = mara_arena_alloc_ex(
			env, &env->permanent_arena,
			string.len, _Alignof(char)
		);
		mara_assert(chars != NULL, "Out of memory");
		memcpy(chars, string.data, string.len);
		new_node->key = (mara_str_t){
			.len = string.len,
			.data = chars,
		};

		if (last_node_index >= 0) {
			symtab->nodes[last_node_index].children[last_hash_itr & BHAMT_MASK] = current_len + 1;
		}

		return current_len;
	}
}

mara_str_t
mara_symtab_lookup(mara_symtab_t* symtab, mara_index_t id) {
	return symtab->nodes[id].key;
}

void
mara_symtab_cleanup(mara_env_t* env, mara_symtab_t* symtab) {
	mara_free(env->options.allocator, symtab->nodes);
	symtab->len = 0;
	symtab->capacity = 0;
	symtab->nodes = NULL;
}
