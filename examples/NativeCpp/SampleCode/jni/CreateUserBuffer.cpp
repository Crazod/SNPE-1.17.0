//==============================================================================
//
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved.
//  Confidential and Proprietary - Qualcomm Technologies, Inc.
//
//==============================================================================

#include <iostream>
#include <vector>
#include <string>
#include <assert.h>
#include <stdexcept>
#include <unordered_map>

#include "CreateUserBuffer.hpp"
#include "Util.hpp"

#include "SNPE/SNPE.hpp"
#include "SNPE/SNPEFactory.hpp"
#include "DlSystem/StringList.hpp"
#include "DlSystem/TensorShape.hpp"
#include "DlSystem/IUserBuffer.hpp"
#include "DlSystem/IUserBufferFactory.hpp"
#include "DlSystem/UserBufferMap.hpp"

void createUserBuffer(zdl::DlSystem::UserBufferMap& userBufferMap,
                      std::unordered_map<std::string, std::vector<uint8_t>>& applicationBuffers,
                      std::vector<std::unique_ptr<zdl::DlSystem::IUserBuffer>>& snpeUserBackedBuffers,
                      std::unique_ptr<zdl::SNPE::SNPE>& snpe,
                      const char * name)
{
   // get attributes of buffer by name
   auto bufferAttributesOpt = snpe->getInputOutputBufferAttributes(name);
   if (!bufferAttributesOpt) throw std::runtime_error(std::string("Error obtaining attributes for input tensor ") + name);

   // calculate the size of buffer required by the input tensor
   const zdl::DlSystem::TensorShape& bufferShape = (*bufferAttributesOpt)->getDims();

   // calculate stride based on buffer strides
   // Note: Strides = Number of bytes to advance to the next element in each dimension.
   // For example, if a float tensor of dimension 2x4x3 is tightly packed in a buffer of 96 bytes, then the strides would be (48,12,4)
   std::vector<size_t> strides(bufferShape.rank());
   strides[strides.size() - 1] = sizeof(float);
   size_t stride = strides[strides.size() - 1];
   for (size_t i = bufferShape.rank() - 1; i > 0; i--)
   {
      stride *= bufferShape[i];
      strides[i-1] = stride;
   }

   const size_t bufferElementSize = (*bufferAttributesOpt)->getElementSize();
   size_t bufSize = calcSizeFromDims(bufferShape.getDimensions(), bufferShape.rank(), bufferElementSize);

   // set the buffer encoding type
   zdl::DlSystem::UserBufferEncodingFloat userBufferEncodingFloat;

   // create user-backed storage to load input data onto it
   applicationBuffers.emplace(name, std::vector<uint8_t>(bufSize));

   // create SNPE user buffer from the user-backed buffer
   zdl::DlSystem::IUserBufferFactory& ubFactory = zdl::SNPE::SNPEFactory::getUserBufferFactory();
   snpeUserBackedBuffers.push_back(ubFactory.createUserBuffer(applicationBuffers.at(name).data(),
                                                              bufSize,
                                                              strides,
                                                              &userBufferEncodingFloat));

   // add the user-backed buffer to the inputMap, which is later on fed to the network for execution
   userBufferMap.add(name, snpeUserBackedBuffers.back().get());
}

void createInputBufferMap(zdl::DlSystem::UserBufferMap& inputMap,
                          std::unordered_map<std::string, std::vector<uint8_t>>& applicationBuffers,
                          std::vector<std::unique_ptr<zdl::DlSystem::IUserBuffer>>& snpeUserBackedBuffers,
                          std::unique_ptr<zdl::SNPE::SNPE>& snpe)
{
   // get input tensor names of the network that need to be populated
   const auto& inputNamesOpt = snpe->getInputTensorNames();
   if (!inputNamesOpt) throw std::runtime_error("Error obtaining input tensor names");
   const zdl::DlSystem::StringList& inputNames = *inputNamesOpt;
   assert(inputNames.size() > 0);

   // create SNPE user buffers for each application storage buffer
   for (const char *name : inputNames) {
      createUserBuffer(inputMap, applicationBuffers, snpeUserBackedBuffers, snpe, name);
   }
}

void createOutputBufferMap(zdl::DlSystem::UserBufferMap& outputMap,
                           std::unordered_map<std::string, std::vector<uint8_t>>& applicationBuffers,
                           std::vector<std::unique_ptr<zdl::DlSystem::IUserBuffer>>& snpeUserBackedBuffers,
                           std::unique_ptr<zdl::SNPE::SNPE>& snpe)
{
   // get input tensor names of the network that need to be populated
   const auto& outputNamesOpt = snpe->getOutputTensorNames();
   if (!outputNamesOpt) throw std::runtime_error("Error obtaining output tensor names");
   const zdl::DlSystem::StringList& outputNames = *outputNamesOpt;

   // create SNPE user buffers for each application storage buffer
   for (const char *name : outputNames) {
      createUserBuffer(outputMap, applicationBuffers, snpeUserBackedBuffers, snpe, name);
   }
}
