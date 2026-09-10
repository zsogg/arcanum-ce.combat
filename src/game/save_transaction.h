#ifndef ARCANUM_GAME_SAVE_TRANSACTION_H_
#define ARCANUM_GAME_SAVE_TRANSACTION_H_

#include <stdbool.h>

// Private save staging, in the same repository/filesystem as the live slots.
#define SAVE_TRANSACTION_DIR "save\\ce-save-transaction"
#define SAVE_TRANSACTION_STAGE SAVE_TRANSACTION_DIR "\\new"

bool save_transaction_begin(void);
bool save_transaction_commit(const char* name, const char* description);
bool save_transaction_recover(void);

#endif
