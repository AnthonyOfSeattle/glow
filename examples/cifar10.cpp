#include "noether/Image.h"
#include "noether/Network.h"
#include "noether/Nodes.h"
#include "noether/Support.h"
#include "noether/Tensor.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

using namespace noether;

/// The CIFAR file format is structured as one byte label in the range 0..9.
/// The label is followed by an image: 32 x 32 pixels, in RGB format. Each
/// color is 1 byte. The first 1024 red bytes are followed by 1024 of green
/// and blue. Each 1024 byte color slice is organized in row-major format.
/// The database contains 10000 images.
/// Size: (1 + (32 * 32 * 3)) * 10000 = 30730000.
const size_t cifarImageSize = 1 + (32 * 32 * 3);
const size_t cifarNumImages = 10000;

/// This test classifies digits from the CIFAR labeled dataset.
/// Details: http://www.cs.toronto.edu/~kriz/cifar.html
/// Dataset: http://www.cs.toronto.edu/~kriz/cifar-10-binary.tar.gz
void testCIFAR10() {
  (void)cifarImageSize;
  const char *textualLabels[] = {"airplane", "automobile", "bird", "cat",
                                 "deer",     "dog",        "frog", "horse",
                                 "ship",     "truck"};

  std::ifstream dbInput("cifar-10-batches-bin/data_batch_1.bin",
                        std::ios::binary);

  std::cout << "Loading the CIFAR-10 database.\n";

  /// Load the CIFAR database into a 4d tensor.
  Tensor images(ElemKind::FloatTy, {cifarNumImages, 32, 32, 3});
  Tensor labels(ElemKind::IndexTy, {cifarNumImages});
  size_t idx = 0;

  auto labelsH = labels.getHandle<size_t>();
  auto imagesH = images.getHandle<FloatTy>();
  for (unsigned w = 0; w < cifarNumImages; w++) {
    labelsH.at({w}) = static_cast<uint8_t>(dbInput.get());
    idx++;

    for (unsigned z = 0; z < 3; z++) {
      for (unsigned y = 0; y < 32; y++) {
        for (unsigned x = 0; x < 32; x++) {
          imagesH.at({w, x, y, z}) =
              FloatTy(static_cast<uint8_t>(dbInput.get())) / 255.0;
          idx++;
        }
      }
    }
  }
  assert(idx == cifarImageSize * cifarNumImages && "Invalid input file");

  // Construct the network:
  Network N;
  N.getConfig().learningRate = 0.001;
  N.getConfig().momentum = 0.9;
  N.getConfig().batchSize = 8;
  N.getConfig().L2Decay = 0.0001;

  auto *A = N.createArrayNode({32, 32, 3});
  auto *CV0 = N.createConvNode(A, 16, 5, 1, 2);
  auto *RL0 = N.createRELUNode(CV0);
  auto *MP0 = N.createMaxPoolNode(RL0, 2, 2, 0);

  auto *CV1 = N.createConvNode(MP0, 20, 5, 1, 2);
  auto *RL1 = N.createRELUNode(CV1);
  auto *MP1 = N.createMaxPoolNode(RL1, 2, 2, 0);

  auto *CV2 = N.createConvNode(MP1, 20, 5, 1, 2);
  auto *RL2 = N.createRELUNode(CV2);
  auto *MP2 = N.createMaxPoolNode(RL2, 2, 2, 0);

  auto *FCL1 = N.createFullyConnectedNode(MP2, 10);
  auto *RL3 = N.createRELUNode(FCL1);
  auto *SM = N.createSoftMaxNode(RL3);

  // Report progress every this number of training iterations.
  int reportRate = 256;

  std::cout << "Training.\n";

  for (int iter = 0; iter < 190 * reportRate; iter++) {
    std::cout << "Training - iteration #" << iter << " ";
    TimerGuard reportTime(reportRate);

    // Bind the images tensor to the input array A, and the labels tensor
    // to the softmax node SM.
    N.train(SM, reportRate / N.getConfig().batchSize, {A, SM},
            {&images, &labels});

    unsigned score = 0;
    for (size_t i = 0; i < 100; i++) {
      // Pick a random image from the stack:
      const unsigned imageIndex = ((i + iter) * 175 + 912) % cifarNumImages;
      // Load the image.
      Tensor sample = imagesH.extractSlice(imageIndex);
      auto *res = N.infer(SM, {A}, {&sample});

      // Read the expected label.
      auto expectedLabel = labelsH.at({imageIndex});
      unsigned result = res->getHandle<FloatTy>().maxArg();
      score += textualLabels[expectedLabel] == textualLabels[result];
    }
    std::cout << "Score : " << score << " / 100.\n";
  }
}

int main() {
  testCIFAR10();

  return 0;
}
