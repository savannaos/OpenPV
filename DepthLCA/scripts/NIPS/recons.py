import os, sys
lib_path = os.path.abspath("/home/slundquist/workspace/PetaVision/plab/")
sys.path.append(lib_path)
from plotRecon import plotRecon
from plotReconError import plotReconError

#For plotting
#import matplotlib.pyplot as plt

outputDir = "/nh/compneuro/Data/Depth/LCA/benchmark/validate/recons_run/"
skipFrames = 1 #Only print every 20th frame
startFrames = 0
doPlotRecon = True
doPlotErr = False
errShowPlots = False
layers = [
   "a0_LeftImage",
   "a2_LeftGanglion",
   "a3_RightImage",
   "a5_RightGanglion",
   "a7_LeftRecon",
   "a8_RightRecon",
   ]

#Layers for constructing recon error
preErrLayers = [
   "a1_LeftDownsample",
   "a5_RightDownsample",
]

postErrLayers = [
   "a3_LeftRecon",
   "a7_RightRecon",
]

gtLayers = None
#gtLayers = [
#   #"a25_DepthRescale",
#   #"a25_DepthRescale",
#   #"a25_DepthRescale",
#   "a25_DepthRescale",
#   "a25_DepthRescale",
#   "a25_DepthRescale",
#]

preToPostScale = [
   .007,
   .007,
]


if(doPlotRecon):
   print("Plotting reconstructions")
   plotRecon(layers, outputDir, skipFrames)

if(doPlotErr):
   print("Plotting reconstruction error")
   plotReconError(preErrLayers, postErrLayers, preToPostScale, outputDir, errShowPlots, skipFrames, gtLayers)
