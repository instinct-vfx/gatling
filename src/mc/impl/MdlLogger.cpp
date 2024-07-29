//
// Copyright (C) 2019-2022 Pablo Delgado Krämer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "MdlLogger.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-copy"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-function"
#include <mi/mdl_sdk.h>
#pragma clang diagnostic pop

#include <stdio.h>
#include <gtl/gb/Log.h>

namespace
{
  const char* _MiMessageKindToCStr(mi::neuraylib::IMessage::Kind kind)
  {
    switch (kind)
    {
    case mi::neuraylib::IMessage::MSG_INTEGRATION:
      return "MDL SDK";
    case mi::neuraylib::IMessage::MSG_IMP_EXP:
      return "Importer/Exporter";
    case mi::neuraylib::IMessage::MSG_COMILER_BACKEND:
      return "Compiler Backend";
    case mi::neuraylib::IMessage::MSG_COMILER_CORE:
      return "Compiler Core";
    case mi::neuraylib::IMessage::MSG_COMPILER_ARCHIVE_TOOL:
      return "Compiler Archive Tool";
    case mi::neuraylib::IMessage::MSG_COMPILER_DAG:
      return "Compiler DAG generator";
    default:
      break;
    }
    return "";
  }
}

namespace gtl
{
  void McMdlLogger::message(mi::base::Message_severity level,
                            [[maybe_unused]] const char* moduleCategory,
                            [[maybe_unused]] const mi::base::Message_details& details,
                            const char* message)
  {
#ifdef NDEBUG
    const mi::base::Message_severity minLogLevel = mi::base::MESSAGE_SEVERITY_ERROR;
#elif defined(GTL_VERBOSE)
    const mi::base::Message_severity minLogLevel = mi::base::MESSAGE_SEVERITY_INFO;
#else
    const mi::base::Message_severity minLogLevel = mi::base::MESSAGE_SEVERITY_WARNING;
#endif

    if (level > minLogLevel)
    {
      return;
    }

    // Ignore log spam from MaterialX MDL code generation.
    // FIXME: use MDL 'warning' execution context option instead
    if (strstr(message, "unused parameter") ||
        strstr(message, "unused variable") ||
        strstr(message, "unused let temporary") ||
        strstr(message, "unreferenced local function"))
    {
      return;
    }

    if (level <= mi::base::MESSAGE_SEVERITY_ERROR)
    {
      GB_ERROR("[MDL] {}", message);
    }
    else if (level <= mi::base::MESSAGE_SEVERITY_WARNING)
    {
      GB_WARN("[MDL] {}", message);
    }
    else
    {
      GB_DEBUG("[MDL] {}", message);
    }
  }

  void McMdlLogger::message(mi::base::Message_severity level,
                            const char* moduleCategory,
                            const char* message)
  {
    this->message(level, moduleCategory, mi::base::Message_details{}, message);
  }

  void McMdlLogger::message(mi::base::Message_severity level,
                            const char* message)
  {
    this->message(level, nullptr, message);
  }

  void McMdlLogger::flushContextMessages(mi::neuraylib::IMdl_execution_context* context)
  {
    for (mi::Size i = 0, n = context->get_messages_count(); i < n; ++i)
    {
      mi::base::Handle<const mi::neuraylib::IMessage> message(context->get_message(i));

      const char* s_msg = message->get_string();
      const char* s_kind = _MiMessageKindToCStr(message->get_kind());
      this->message(message->get_severity(), s_kind, s_msg);
    }
    context->clear_messages();
  }
}
