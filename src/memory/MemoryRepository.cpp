#include "memory/MemoryRepository.h"

namespace zhu_screen_pet {
namespace {
void copyError(const AppError& error, QString* output)
{ if (output) *output = error.technicalMessage.isEmpty() ? error.message : error.technicalMessage; }
}
qint64 MemoryRepository::save(const MemoryItem& item, QString* e)
{ const auto r=saveResult(item); if(!r)copyError(r.error(),e); return r?r.value():0; }
qint64 MemoryRepository::saveShortTerm(const QString& c,const QString& s,const QDateTime& x,QString* e)
{ const auto r=saveShortTermResult(c,s,x); if(!r)copyError(r.error(),e); return r?r.value():0; }
qint64 MemoryRepository::saveLongTerm(const QString& c,const QString& s,QString* e)
{ const auto r=saveLongTermResult(c,s); if(!r)copyError(r.error(),e); return r?r.value():0; }
bool MemoryRepository::remove(qint64 id,QString* e)
{ const auto r=removeResult(id); if(!r)copyError(r.error(),e); return r.succeeded(); }
QVector<MemoryItem> MemoryRepository::search(const QString& q,int l,QString* e) const
{ const auto r=searchResult(q,l); if(!r)copyError(r.error(),e); return r?r.value():QVector<MemoryItem>{}; }
QVector<ConversationMessage> MemoryRepository::searchConversationMessages(const QString& q,int l,QString* e) const
{ const auto r=searchConversationMessagesResult(q,l); if(!r)copyError(r.error(),e); return r?r.value():QVector<ConversationMessage>{}; }
QVector<MemoryItem> MemoryRepository::searchShortTerm(const QString& q,int l,QString* e) const
{ const auto r=searchShortTermResult(q,l); if(!r)copyError(r.error(),e); return r?r.value():QVector<MemoryItem>{}; }
QVector<MemoryItem> MemoryRepository::searchLongTerm(const QString& q,int l,QString* e) const
{ const auto r=searchLongTermResult(q,l); if(!r)copyError(r.error(),e); return r?r.value():QVector<MemoryItem>{}; }
} // namespace zhu_screen_pet
