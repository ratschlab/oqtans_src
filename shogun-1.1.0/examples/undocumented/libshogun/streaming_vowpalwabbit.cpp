/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Written (W) 2011 Shashwat Lal Das
 * Copyright (C) 2011 Berlin Institute of Technology and Max-Planck-Society
 *
 * This example demonstrates use of the Vowpal Wabbit learning algorithm.
 */

#include <shogun/lib/common.h>

#include <shogun/io/StreamingVwFile.h>
#include <shogun/features/StreamingVwFeatures.h>
#include <shogun/classifier/vw/VowpalWabbit.h>

using namespace shogun;

int main()
{
	init_shogun_with_defaults();

	char* train_file_name = "../data/train_sparsereal.light";
	CStreamingVwFile* train_file = new CStreamingVwFile(train_file_name);
	train_file->set_parser_type(T_SVMLIGHT); // Treat the file as SVMLight format
	SG_REF(train_file);

	CStreamingVwFeatures* train_features = new CStreamingVwFeatures(train_file, true, 1024);
	SG_REF(train_features);

	CVowpalWabbit* vw = new CVowpalWabbit(train_features);
	vw->set_regressor_out("./vw_regressor_text.dat"); // Save regressor to this file
	vw->set_adaptive(false);			  // Use adaptive learning
	vw->train_machine();

	SG_SPRINT("Weights have been output in text form to vw_regressor_text.dat.\n");
	train_file->close();

	SG_UNREF(train_features);
	SG_UNREF(train_file);
	SG_UNREF(vw);

	exit_shogun();

	return 0;
}
