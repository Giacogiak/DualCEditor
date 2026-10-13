#include "export_job.h"

#include <QElapsedTimer>
#include <QThread>

#include <algorithm>

ExportJob::ExportJob(std::shared_ptr<const ExportField> field, ExportSpec spec, QObject* parent)
    : QObject(parent), field_(std::move(field)), spec_(std::move(spec)) {}

ExportJob::~ExportJob() {
  if (!thread_) return;
  cancel_.request();
  thread_->wait();
  delete thread_;
}

void ExportJob::start() {
  if (thread_) return;
  thread_ = QThread::create([this] {
    QElapsedTimer t;
    t.start();
    try {
      result_ = runExport(field_->field(), spec_, &cancel_, this);
    } catch (const std::exception& e) {
      result_.status = ExportStatus::Failed;
      result_.error = e.what();
    }
    ms_ = double(t.nsecsElapsed()) / 1e6;
  });
  connect(thread_, &QThread::finished, this, [this] {
    thread_->wait();
    emit finished();
  });
  thread_->start();
}

void ExportJob::cancel() { cancel_.request(); }

bool ExportJob::running() const { return thread_ && thread_->isRunning(); }

QString ExportJob::stageText(int stage, qulonglong done, qulonglong total) {
  switch (dualc::Stage(stage)) {
    case dualc::Stage::Sample: return tr("Sampling");
    case dualc::Stage::Contour: return tr("Contouring");
    case dualc::Stage::Write: return tr("Writing");
    case dualc::Stage::Tile:
      return done >= total ? tr("Writing") : tr("Tile %1 of %2").arg(done + 1).arg(total);
  }
  return {};
}

int ExportJob::permille(int stage, qulonglong done, qulonglong total) {
  const double f = total ? std::min(1.0, double(done) / double(total)) : 0.0;
  switch (dualc::Stage(stage)) {
    case dualc::Stage::Sample: return int(450 * f);
    case dualc::Stage::Contour: return 450 + int(500 * f);
    case dualc::Stage::Write: return 950 + int(50 * f);
    case dualc::Stage::Tile: return int(1000 * f);
  }
  return 0;
}

void ExportJob::report(dualc::Stage stage, std::size_t done, std::size_t total) noexcept {
  QMetaObject::invokeMethod(
      this,
      [this, s = int(stage), done, total] {
        emit progress(permille(s, done, total), stageText(s, done, total));
      },
      Qt::QueuedConnection);
}
