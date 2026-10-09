// Runs `DualCEditor --snapshot PATH` and checks the PNG: 800x600, more than
// one colour, and geometry on it. The background shader is a vertical
// gradient, so every background row is one colour; a pixel that differs from
// its row's left-edge pixel is the traced surface. Exits 77 (ctest: skipped)
// when the editor reports no OpenGL 3.3 core context.
//
// usage: snapshot_test <DualCEditor> <out.png>

#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QSet>

#include <cstdio>
#include <cstdlib>

namespace {

constexpr int kExitSkip = 77;

int fail(const char* what) {
  std::fprintf(stderr, "snapshot_test: %s\n", what);
  return 1;
}

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  if (argc != 3) return fail("usage: snapshot_test <DualCEditor> <out.png>");
  const QString editor = QString::fromLocal8Bit(argv[1]);
  const QString out = QString::fromLocal8Bit(argv[2]);
  QFile::remove(out);

  QProcess proc;
  proc.setProcessChannelMode(QProcess::ForwardedChannels);
  proc.start(editor, {"--snapshot", out});
  if (!proc.waitForStarted()) return fail("cannot start the editor");
  if (!proc.waitForFinished(600000)) return fail("the editor timed out");
  if (proc.exitStatus() != QProcess::NormalExit) return fail("the editor crashed");
  if (proc.exitCode() == kExitSkip) {
    std::printf("snapshot_test: skipped, no OpenGL 3.3 core context\n");
    return kExitSkip;
  }
  if (proc.exitCode() != 0) return fail("the editor exited non-zero");

  QImage image;
  if (!image.load(out, "PNG")) return fail("the snapshot is not a readable PNG");
  if (image.width() != 800 || image.height() != 600)
    return fail("the snapshot is not 800x600");
  image = image.convertToFormat(QImage::Format_RGB32);

  QSet<QRgb> colours;
  long long surface = 0;
  for (int y = 0; y < image.height(); ++y) {
    const QRgb* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
    const QRgb edge = row[0];
    for (int x = 0; x < image.width(); ++x) {
      colours.insert(row[x]);
      const int d = std::abs(qRed(row[x]) - qRed(edge)) +
                    std::abs(qGreen(row[x]) - qGreen(edge)) +
                    std::abs(qBlue(row[x]) - qBlue(edge));
      if (d > 12) ++surface;
    }
  }
  const double surfaceShare = double(surface) / (image.width() * image.height());
  std::printf("snapshot_test: %lld colours, surface covers %.1f%% of the frame\n",
              static_cast<long long>(colours.size()), 100.0 * surfaceShare);
  if (colours.size() < 2) return fail("the snapshot is a single flat colour");
  if (surfaceShare < 0.05) return fail("the snapshot shows no surface");
  return 0;
}
