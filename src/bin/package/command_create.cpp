/*
 * Copyright 2009, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Distributed under the terms of the MIT License.
 */


#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Entry.h>

#include <package/PackageInfo.h>
#include <package/hpkg/HPKGDefs.h>
#include <package/hpkg/PackageWriter.h>

#include "package.h"
#include "StandardErrorOutput.h"


using BPackageKit::BHPKG::BPackageWriterListener;
using BPackageKit::BHPKG::BPackageWriter;


class PackageWriterListener	: public BPackageWriterListener {
public:
	PackageWriterListener(bool verbose, bool quiet)
		: fVerbose(verbose), fQuiet(quiet)
	{
	}

	virtual void PrintErrorVarArgs(const char* format, va_list args)
	{
		vfprintf(stderr, format, args);
	}

	virtual void OnEntryAdded(const char* path)
	{
		if (fQuiet || !fVerbose)
			return;

		printf("\t%s\n", path);
	}

	virtual void OnTOCSizeInfo(uint64 uncompressedAttributeTypesSize,
		uint64 uncompressedStringsSize, uint64 uncompressedMainSize,
		uint64 uncompressedTOCSize)
	{
		if (fQuiet || !fVerbose)
			return;

		printf("----- TOC Info -----\n");
		printf("attribute types size:    %10llu (uncompressed)\n",
			uncompressedAttributeTypesSize);
		printf("cached strings size:     %10llu (uncompressed)\n",
			uncompressedStringsSize);
		printf("TOC main size:           %10llu (uncompressed)\n",
			uncompressedMainSize);
		printf("total TOC size:          %10llu (uncompressed)\n",
			uncompressedTOCSize);
	}

	virtual void OnPackageAttributesSizeInfo(uint32 uncompressedSize)
	{
		if (fQuiet || !fVerbose)
			return;

		printf("----- Package Attribute Info -----\n");
		printf("package attributes size: %10ld (uncompressed)\n",
			uncompressedSize);
	}

	virtual void OnPackageSizeInfo(uint32 headerSize, uint64 heapSize,
		uint64 tocSize, uint32 packageAttributesSize, uint64 totalSize)
	{
		if (fQuiet)
			return;

		printf("----- Package Info -----\n");
		printf("header size:             %10lu\n", headerSize);
		printf("heap size:               %10lld\n", heapSize);
		printf("TOC size:                %10lld\n", tocSize);
		printf("package attributes size: %10ld\n", packageAttributesSize);
		printf("total size:              %10lld\n", totalSize);

	}

private:
	bool fVerbose;
	bool fQuiet;
};


int
command_create(int argc, const char* const* argv)
{
	const char* changeToDirectory = NULL;
	bool quiet = false;
	bool verbose = false;

	while (true) {
		static struct option sLongOptions[] = {
			{ "help", no_argument, 0, 'h' },
			{ "quiet", no_argument, 0, 'q' },
			{ "verbose", no_argument, 0, 'v' },
			{ 0, 0, 0, 0 }
		};

		opterr = 0; // don't print errors
		int c = getopt_long(argc, (char**)argv, "+C:hqv", sLongOptions, NULL);
		if (c == -1)
			break;

		switch (c) {
			case 'C':
				changeToDirectory = optarg;
				break;

			case 'h':
				print_usage_and_exit(false);
				break;

			case 'q':
				quiet = true;
				break;

			case 'v':
				verbose = true;
				break;

			default:
				print_usage_and_exit(true);
				break;
		}
	}

	// The remaining arguments are the package file and the list of files to
	// include, i.e. at least two more arguments.
	if (optind + 2 > argc)
		print_usage_and_exit(true);

	const char* packageFileName = argv[optind++];
	const char* const* fileNames = argv + optind;
	int fileNameCount = argc - optind;

	// create package
	PackageWriterListener listener(verbose, quiet);
	BPackageWriter packageWriter(&listener);
	status_t result = packageWriter.Init(packageFileName);
	if (result != B_OK)
		return 1;

	// change directory, if requested
	if (changeToDirectory != NULL) {
		if (chdir(changeToDirectory) != 0) {
			listener.PrintError(
				"Error: Failed to change the current working directory to "
				"\"%s\": %s\n", changeToDirectory, strerror(errno));
		}
	}

	// add files
	for (int i = 0; i < fileNameCount; i++) {
		if (strcmp(fileNames[i], ".") == 0) {
			DIR* dir = opendir(".");
			if (dir == NULL) {
				listener.PrintError("Error: Failed to opendir '.': %s\n",
					strerror(errno));
				return 1;
			}
			while (dirent* entry = readdir(dir)) {
				if (strcmp(entry->d_name, ".") == 0
					|| strcmp(entry->d_name, "..") == 0)
					continue;

				result = packageWriter.AddEntry(entry->d_name);
				if (result != B_OK)
					return 1;
			}
			closedir(dir);

		} else {
			result = packageWriter.AddEntry(fileNames[i]);
			if (result != B_OK)
				return 1;
		}
	}

	// write the package
	result = packageWriter.Finish();
	if (result != B_OK)
		return 1;

	if (verbose)
		printf("\nsuccessfully created package '%s'\n", packageFileName);

	return 0;
}
