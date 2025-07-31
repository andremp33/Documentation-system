#ifndef INDEX_H
#define INDEX_H

extern char document_folder[256];

int index_add(const char *title, const char *authors, const char *year, const char *path);
DocumentMeta* index_query(int id);
int index_remove(int id);
int index_load(const char *filename);
int index_save(const char *filename);
int index_total();
DocumentMeta* index_get(int i);
int index_save(const char *filename);
int index_load(const char *filename);
int index_get_count();
int extract_metadata(const char *filepath, char *title, size_t max_title, char *author, size_t max_author);

#endif