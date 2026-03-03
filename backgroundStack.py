#!/usr/bin/python

import ROOT
from ROOT import gROOT
from ROOT import TH1
from ROOT import TFile
from ROOT import THStack
from ROOT import TCanvas
from ROOT import TPad
from ROOT import TPaveText

import PlotUtils
from ROOT import MnvColors

gROOT.SetBatch() #Don't render histograms to a window.  Also gets filled areas correct.

var = "pTmu"
dataName = var + "_data"

ratioMin = 0.7
ratioMax = 1.3

bottomFraction = 0.2
margin = 0.078 #Tuned by hand
labelSize = 0.15
lineSize = 3

TH1.AddDirectory(False)
dataFile = TFile.Open("runEventLoopData.root")
mcFile = TFile.Open("runEventLoopMC.root")

mcPOT = mcFile.Get("POTUsed").GetVal()
dataPOT = dataFile.Get("POTUsed").GetVal()

#Organize the MC backgrounds into a stacked histogram.
#Also keep a sum of backgrounds that has full systematics
#information.
mcStack = THStack()
signalHist = mcFile.Get(var + "_selected_signal_reco")
signalHist.Scale(dataPOT/mcPOT)
mcSum = signalHist.Clone()
for key in mcFile.GetListOfKeys():
  name = str(key.GetName())
  if name.find("background") > -1 and name.find(var) > -1:
    hist = key.ReadObj()
    hist.Scale(dataPOT/mcPOT)
    mcStack.Add(hist.GetCVHistoWithError().Clone())
    mcSum.Add(hist)
mcStack.Add(signalHist.GetCVHistoWithError().Clone())

#Apply a different color for each MC category
mcColors = MnvColors.GetColors(MnvColors.kOkabeItoDarkPalette)
nextColor = 0
for hist in mcStack.GetHists():
  hist.SetLineColor(mcColors[nextColor])
  hist.SetFillColor(mcColors[nextColor])
  nextColor = nextColor + 1

dataHist = dataFile.Get(dataName)
dataWithStatErrors = dataHist.GetCVHistoWithError().Clone()
dataHist.AddMissingErrorBandsAndFillWithCV(mcFile.Get(var + "_selected_signal_reco"))

#Create a TCanvas on which to draw plots and split it into 2 panels
overall = TCanvas("Data/MC for " + var)
top = TPad("Overlay", "Overlay", 0, bottomFraction, 1, 1)
bottom = TPad("Ratio", "Ratio", 0, 0, 1, bottomFraction + margin)
#Thou shalt Draw() new TPads lest they be blank!
top.Draw()
top.SetLogy()
bottom.Draw()

top.cd()
mcTotal = mcStack.GetStack().Last()
mcTotal.Draw("E2") #Draw the error envelope only.  Not used here except to force the THStack to calculate its sum.

mcStack.SetMinimum(1)
mcStack.SetMaximum(mcTotal.GetMaximum() * 10) #Make room for annotations above the plot
mcStack.Draw("HIST")

dataWithStatErrors.SetLineColor(ROOT.kBlack)
dataWithStatErrors.SetLineWidth(lineSize)
dataWithStatErrors.SetMarkerStyle(20) #Resizeable closed circle
dataWithStatErrors.SetMarkerColor(ROOT.kBlack)
dataWithStatErrors.SetMarkerSize(0.7)
dataWithStatErrors.SetTitle("Data")
dataWithStatErrors.Draw("SAME")

legend = top.BuildLegend(0.5, 0.4, 0.9, 0.9) #TODO: Explain legend position.  ROOT 6 might even make an intelligent decision about where to place it.

#Data/MC ratio panel
bottom.cd()
bottom.SetTopMargin(0)
bottom.SetBottomMargin(0.3)

ratio = dataHist.Clone()
mcTotalWithSys = mcSum
ratio.Divide(ratio, mcTotalWithSys)

#TODO: I need GetCVHistoWithError() from mcRatio, but THStack doesn't keep a MnvH1D.  I have to Add() the histograms myself.

#Now fill mcRatio with 1 for bin content and fractional error
mcRatio = mcTotalWithSys.GetTotalError(False, True, False) #The second "true" makes this fractional error
for whichBin in range(1, mcRatio.GetXaxis().GetNbins()+1):
  mcRatio.SetBinError(whichBin, max(mcRatio.GetBinContent(whichBin), 1e-9))
  mcRatio.SetBinContent(whichBin, 1)

ratio.SetTitle("")
ratio.SetLineColor(ROOT.kBlack)
ratio.SetLineWidth(lineSize)
ratio.SetTitleSize(0)

ratio.GetYaxis().SetTitle("Data / MC")
ratio.GetYaxis().SetLabelSize(labelSize)
ratio.GetYaxis().SetTitleSize(0.16)
ratio.GetYaxis().SetTitleOffset(0.2)
ratio.GetYaxis().SetNdivisions(505) #5 minor divisions between 5 major divisions.  I'm trying to match a specific paper here.

ratio.GetXaxis().SetTitleSize(0.16)
ratio.GetXaxis().SetTitleOffset(0.9)
ratio.GetXaxis().SetLabelSize(labelSize)

ratio.SetMinimum(ratioMin)
ratio.SetMaximum(ratioMax)
ratio.Draw()

#Error envelope for the MC
mcRatio.SetLineColor(ROOT.kRed)
mcRatio.SetLineWidth(lineSize)
mcRatio.SetFillColorAlpha(ROOT.kPink + 1, 0.4)
mcRatio.Draw("E2 SAME")

#Draw a flat line at 1 for ratio of MC to itself
straightLine = mcRatio.Clone()
straightLine.SetFillStyle(0)
straightLine.Draw("HIST SAME")

#Title for the whole plot
top.cd()
title = TPaveText(0.3, 0.91, 0.7, 1.0, "nbNDC") #no border and use Normalized Device Coordinates to place it
title.SetFillStyle(0)
title.SetLineColor(ROOT.kWhite)
title.AddText("CC Inclusive Event Rate")
title.Draw()

plotter = PlotUtils.MnvPlotter()
plotter.WritePreliminary(0.3, 0.82, 5e-2, True)

#Make a PNG file of this canvas
overall.Print(var + "DataMCRatio.png")
