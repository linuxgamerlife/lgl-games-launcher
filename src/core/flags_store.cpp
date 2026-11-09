#include "core/flags_store.h"

#include "core/paths.h"

#include <glib.h>
#include <sqlite3.h>

#include <utility>

namespace gameslauncher {
namespace {

bool step_done(sqlite3_stmt* statement) {
  return sqlite3_step(statement) == SQLITE_DONE;
}

} // namespace

FlagsStore::~FlagsStore() {
  if (m_db != nullptr) {
    sqlite3_close(m_db);
    m_db = nullptr;
  }
}

bool FlagsStore::open(const std::string& path, std::string* error) {
  if (m_db != nullptr) {
    return true;
  }
  if (path != ":memory:" && !ensure_private_dir(data_dir())) {
    if (error != nullptr) {
      *error = "could not create the data directory";
    }
    return false;
  }
  const int result = sqlite3_open_v2(path.c_str(), &m_db,
                                     SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
  if (result != SQLITE_OK) {
    if (error != nullptr) {
      *error = m_db != nullptr ? sqlite3_errmsg(m_db) : "could not open the database";
    }
    if (m_db != nullptr) {
      sqlite3_close(m_db);
      m_db = nullptr;
    }
    return false;
  }
  sqlite3_busy_timeout(m_db, 5000);
  if (!ensure_schema(error)) {
    sqlite3_close(m_db);
    m_db = nullptr;
    return false;
  }
  return true;
}

bool FlagsStore::ensure_schema(std::string* error) {
  const char* schema =
      "CREATE TABLE IF NOT EXISTS steam_flags ("
      "app_id TEXT PRIMARY KEY NOT NULL,"
      "favorite INTEGER NOT NULL DEFAULT 0,"
      "hidden INTEGER NOT NULL DEFAULT 0,"
      "tags TEXT NOT NULL DEFAULT '',"
      "status TEXT NOT NULL DEFAULT ''"
      ")";
  char* message = nullptr;
  const int result = sqlite3_exec(m_db, schema, nullptr, nullptr, &message);
  if (result != SQLITE_OK) {
    if (error != nullptr) {
      *error = message != nullptr ? message : "could not create the flags table";
    }
    sqlite3_free(message);
    return false;
  }
  // Existing MVP databases predate the status column.
  sqlite3_exec(m_db, "ALTER TABLE steam_flags ADD COLUMN status TEXT NOT NULL DEFAULT ''",
               nullptr, nullptr, nullptr);
  return true;
}

GameFlags FlagsStore::get(const std::string& app_id) const {
  GameFlags flags;
  if (m_db == nullptr || app_id.empty()) {
    return flags;
  }
  sqlite3_stmt* statement = nullptr;
  const char* sql = "SELECT favorite, hidden, tags, status FROM steam_flags WHERE app_id = ?1";
  if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK) {
    return flags;
  }
  sqlite3_bind_text(statement, 1, app_id.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement) == SQLITE_ROW) {
    flags.favorite = sqlite3_column_int(statement, 0) != 0;
    flags.hidden = sqlite3_column_int(statement, 1) != 0;
    const unsigned char* tags = sqlite3_column_text(statement, 2);
    if (tags != nullptr) {
      flags.tags = reinterpret_cast<const char*>(tags);
    }
    const unsigned char* status = sqlite3_column_text(statement, 3);
    if (status != nullptr) {
      flags.status = reinterpret_cast<const char*>(status);
    }
  }
  sqlite3_finalize(statement);
  return flags;
}

bool FlagsStore::set(const std::string& app_id, const GameFlags& flags) {
  if (m_db == nullptr || app_id.empty() || flags.tags.size() > 4096) {
    return false;
  }
  sqlite3_stmt* statement = nullptr;
  const char* sql =
      "INSERT INTO steam_flags(app_id, favorite, hidden, tags, status) "
      "VALUES(?1, ?2, ?3, ?4, ?5) "
      "ON CONFLICT(app_id) DO UPDATE SET favorite = excluded.favorite, "
      "hidden = excluded.hidden, tags = excluded.tags, status = excluded.status";
  if (sqlite3_prepare_v2(m_db, sql, -1, &statement, nullptr) != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_text(statement, 1, app_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(statement, 2, flags.favorite ? 1 : 0);
  sqlite3_bind_int(statement, 3, flags.hidden ? 1 : 0);
  sqlite3_bind_text(statement, 4, flags.tags.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement, 5, flags.status.c_str(), -1, SQLITE_TRANSIENT);
  const bool ok = step_done(statement);
  sqlite3_finalize(statement);
  return ok;
}

} // namespace gameslauncher
