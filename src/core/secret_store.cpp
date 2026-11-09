#include "core/secret_store.h"

#include <glib.h>
#include <libsecret/secret.h>

namespace gameslauncher {
namespace {

const SecretSchema kSchema = {
    "com.linuxgamerlife.lgl-games-launcher",
    SECRET_SCHEMA_NONE,
    {
        {"purpose", SECRET_SCHEMA_ATTRIBUTE_STRING},
        {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING},
    },
    0,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

} // namespace

std::string SecretStore::lookup(const std::string& purpose) {
  GError* error = nullptr;
  gchar* value = secret_password_lookup_sync(&kSchema, nullptr, &error, "purpose",
                                             purpose.c_str(), nullptr);
  if (error != nullptr) {
    g_warning("Secret Service lookup failed: %s", error->message);
    g_error_free(error);
  }
  if (value == nullptr) {
    return {};
  }
  std::string result(value);
  secret_password_free(value);
  return result;
}

bool SecretStore::store(const std::string& purpose, const std::string& value,
                        std::string* error) {
  GError* failure = nullptr;
  const gboolean ok = secret_password_store_sync(
      &kSchema, SECRET_COLLECTION_DEFAULT, "LGL Games Launcher",
      value.c_str(), nullptr, &failure, "purpose", purpose.c_str(), nullptr);
  if (!ok) {
    if (error != nullptr) {
      *error = failure != nullptr ? failure->message : "could not store the secret";
    }
    if (failure != nullptr) {
      g_error_free(failure);
    }
    return false;
  }
  return true;
}

bool SecretStore::remove(const std::string& purpose, std::string* error) {
  GError* failure = nullptr;
  const gboolean ok = secret_password_clear_sync(&kSchema, nullptr, &failure, "purpose",
                                                 purpose.c_str(), nullptr);
  if (!ok) {
    if (error != nullptr) {
      *error = failure != nullptr ? failure->message : "could not remove the secret";
    }
    if (failure != nullptr) {
      g_error_free(failure);
    }
    return false;
  }
  return true;
}

} // namespace gameslauncher
