#pragma once

#include "mesh_export.h"

#include <QObject>
#include <QString>

#include <memory>

class QThread;

// One export on a worker thread (P06 T1): DualC contours and writes there,
// reports through `progress` to the thread that started the job, and leaves
// its result for `finished`. The event loop of the starting thread runs on
// meanwhile, so the window stays responsive. Cancel requests DualC's token:
// the writer stops at its next checkpoint and leaves no file (the `.part`
// rename). Destroying a running job cancels it and waits for the thread.
class ExportJob : public QObject, private dualc::ProgressSink {
  Q_OBJECT

 public:
  ExportJob(std::shared_ptr<const ExportField> field, ExportSpec spec, QObject* parent = nullptr);
  ~ExportJob() override;

  void start();
  void cancel();
  bool cancelRequested() const { return cancel_.requested(); }
  bool running() const;
  const ExportSpec& spec() const { return spec_; }
  // Valid once `finished` was emitted.
  const ExportResult& result() const { return result_; }
  double ms() const { return ms_; }

  // "Sampling", "Contouring", "Writing", "Tile 3 of 64".
  static QString stageText(int stage, qulonglong done, qulonglong total);
  // The share of the whole job, 0 to 1000: Sample is the first 45%, Contour
  // the next 50%, Write the last 5%; a tiled export is its tile count.
  static int permille(int stage, qulonglong done, qulonglong total);

 signals:
  void progress(int permille, const QString& stage);
  void finished();

 private:
  void report(dualc::Stage stage, std::size_t done, std::size_t total) noexcept override;

  std::shared_ptr<const ExportField> field_;
  ExportSpec spec_;
  dualc::CancelToken cancel_;
  QThread* thread_ = nullptr;
  // Written by the thread before it finishes, read after.
  ExportResult result_;
  double ms_ = 0.0;
};
