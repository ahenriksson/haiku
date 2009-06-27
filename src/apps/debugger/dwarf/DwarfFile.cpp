/*
 * Copyright 2009, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Distributed under the terms of the MIT License.
 */

#include "DwarfFile.h"

#include <new>

#include <AutoDeleter.h>

#include "AttributeClasses.h"
#include "AttributeValue.h"
#include "AbbreviationTable.h"
#include "CompilationUnit.h"
#include "DataReader.h"
#include "ElfFile.h"
#include "TagNames.h"


DwarfFile::DwarfFile()
	:
	fName(NULL),
	fElfFile(NULL),
	fDebugInfoSection(NULL),
	fDebugAbbrevSection(NULL),
	fDebugStringSection(NULL),
	fCurrentCompilationUnit(NULL),
	fFinished(false),
	fFinishError(B_OK)
{
}


DwarfFile::~DwarfFile()
{
	while (CompilationUnit* unit = fCompilationUnits.RemoveHead())
		delete unit;

	while (AbbreviationTable* table = fAbbreviationTables.RemoveHead())
		delete table;

	if (fElfFile != NULL) {
		fElfFile->PutSection(fDebugInfoSection);
		fElfFile->PutSection(fDebugAbbrevSection);
		fElfFile->PutSection(fDebugStringSection);
		delete fElfFile;
	}

	free(fName);
}


status_t
DwarfFile::Load(const char* fileName)
{
	fName = strdup(fileName);
	if (fName == NULL)
		return B_NO_MEMORY;

	// load the ELF file
	fElfFile = new(std::nothrow) ElfFile;
	if (fElfFile == NULL)
		return B_NO_MEMORY;

	status_t error = fElfFile->Init(fileName);
	if (error != B_OK)
		return error;

	// get the interesting sections
	fDebugInfoSection = fElfFile->GetSection(".debug_info");
	fDebugAbbrevSection = fElfFile->GetSection(".debug_abbrev");
	fDebugStringSection = fElfFile->GetSection(".debug_str");
	if (fDebugInfoSection == NULL || fDebugAbbrevSection == NULL
		|| fDebugStringSection == NULL) {
		fprintf(stderr, "DwarfManager::File::Load(\"%s\"): no "
			".debug_info, .debug_abbrev, or .debug_str section.\n",
			fileName);
		return B_ERROR;
	}

	// iterate through the debug info section
	DataReader dataReader(fDebugInfoSection->Data(),
		fDebugInfoSection->Size());
	while (dataReader.HasData()) {
		dwarf_off_t unitHeaderOffset = dataReader.Offset();
		uint64 unitLength = dataReader.Read<uint32>(0);
		bool dwarf64 = (unitLength == 0xffffffff);
		if (dwarf64)
			unitLength = dataReader.Read<uint64>(0);

		dwarf_off_t unitLengthOffset = dataReader.Offset();
			// the unitLength starts here

		if (unitLengthOffset + unitLength
				> (uint64)fDebugInfoSection->Size()) {
			printf("\"%s\": Invalid compilation unit length.\n", fileName);
			break;
		}

		int version = dataReader.Read<uint16>(0);
		off_t abbrevOffset = dwarf64
			? dataReader.Read<uint64>(0)
			: dataReader.Read<uint32>(0);
		uint8 addressSize = dataReader.Read<uint8>(0);

		if (dataReader.HasOverflow()) {
			printf("\"%s\": Unexpected end of data in compilation unit "
				"header.\n", fileName);
			break;
		}

		printf("DWARF%d compilation unit: version %d, length: %lld, "
			"abbrevOffset: %lld, address size: %d\n", dwarf64 ? 64 : 32,
			version, unitLength, abbrevOffset, addressSize);

		if (version != 2 && version != 3) {
			printf("\"%s\": Unsupported compilation unit version: %d\n",
				fileName, version);
			break;
		}

		if (addressSize != 4) {
			printf("\"%s\": Unsupported address size: %d\n", fileName,
				addressSize);
			break;
		}

		dwarf_off_t unitContentOffset = dataReader.Offset();

		// create a compilation unit object
		CompilationUnit* unit = new(std::nothrow) CompilationUnit(
			unitHeaderOffset, unitContentOffset,
			unitLength + (unitLengthOffset - unitHeaderOffset),
			abbrevOffset);
		if (unit == NULL)
			return B_NO_MEMORY;

		fCompilationUnits.Add(unit);

		// parse the debug info for the unit
		fCurrentCompilationUnit = unit;
		error = _ParseCompilationUnit(unit);
		if (error != B_OK)
			return error;

		dataReader.SeekAbsolute(unitLengthOffset + unitLength);
	}

	return B_OK;
}


status_t
DwarfFile::FinishLoading()
{
	if (fFinished)
		return B_OK;
	if (fFinishError != B_OK)
		return fFinishError;

	for (CompilationUnitList::Iterator it = fCompilationUnits.GetIterator();
			CompilationUnit* unit = it.Next();) {
		fCurrentCompilationUnit = unit;
		status_t error = _FinishCompilationUnit(unit);
		if (error != B_OK)
			return fFinishError = error;
	}

	fFinished = true;
	return B_OK;
}


status_t
DwarfFile::_ParseCompilationUnit(CompilationUnit* unit)
{
	AbbreviationTable* abbreviationTable;
	status_t error = _GetAbbreviationTable(unit->AbbreviationOffset(),
		abbreviationTable);
	if (error != B_OK)
		return error;

	unit->SetAbbreviationTable(abbreviationTable);

	DataReader dataReader(
		(const uint8*)fDebugInfoSection->Data() + unit->ContentOffset(),
		unit->ContentSize());

	DebugInfoEntry* entry;
	bool endOfEntryList;
	error = _ParseDebugInfoEntry(dataReader, abbreviationTable, entry,
		endOfEntryList);
	if (error != B_OK)
		return error;

	if (dynamic_cast<DIECompileUnitBase*>(entry) == NULL) {
		fprintf(stderr, "No compilation unit entry in .debug_info "
			"section.\n");
		return B_BAD_DATA;
	}

printf("remaining bytes in unit: %lld\n", dataReader.BytesRemaining());
if (dataReader.HasData()) {
printf("  ");
while (dataReader.HasData()) {
printf("%02x", dataReader.Read<uint8>(0));
}
printf("\n");
}
	return B_OK;
}


status_t
DwarfFile::_ParseDebugInfoEntry(DataReader& dataReader,
	AbbreviationTable* abbreviationTable, DebugInfoEntry*& _entry,
	bool& _endOfEntryList, int level)
{
	dwarf_off_t entryOffset = dataReader.Offset()
		+ fCurrentCompilationUnit->RelativeContentOffset();

	uint32 code = dataReader.ReadUnsignedLEB128(0);
	if (code == 0) {
		if (dataReader.HasOverflow()) {
			fprintf(stderr, "Unexpected end of .debug_info section.\n");
			return B_BAD_DATA;
		}
		_entry = NULL;
		_endOfEntryList = true;
		return B_OK;
	}

	// get the corresponding abbreviation entry
	AbbreviationEntry abbreviationEntry;
	if (!abbreviationTable->GetAbbreviationEntry(code, abbreviationEntry)) {
		fprintf(stderr, "No abbreviation entry for code %lu\n", code);
		return B_BAD_DATA;
	}
printf("%*sentry at %lu: %lu, tag: %s (%lu), children: %d\n", level * 2, "",
entryOffset, abbreviationEntry.Code(), get_entry_tag_name(abbreviationEntry.Tag()),
abbreviationEntry.Tag(), abbreviationEntry.HasChildren());

	DebugInfoEntry* entry;
	status_t error = fDebugInfoFactory.CreateDebugInfoEntry(
		abbreviationEntry.Tag(), entry);
	if (error != B_OK)
		return error;
	ObjectDeleter<DebugInfoEntry> entryDeleter(entry);

	error = fCurrentCompilationUnit->AddDebugInfoEntry(entry, entryOffset);
	if (error != B_OK)
		return error;

	// parse the attributes (supply NULL entry to avoid adding them yet)
	error = _ParseEntryAttributes(dataReader, NULL, abbreviationEntry);
	if (error != B_OK)
		return error;

	// parse children, if the entry has any
	if (abbreviationEntry.HasChildren()) {
		while (true) {
			DebugInfoEntry* childEntry;
			bool endOfEntryList;
			status_t error = _ParseDebugInfoEntry(dataReader,
				abbreviationTable, childEntry, endOfEntryList, level + 1);
			if (error != B_OK)
				return error;

			// add the child to our entry
			if (childEntry != NULL) {
				if (entry != NULL) {
					error = entry->AddChild(childEntry);
					if (error == ENTRY_NOT_HANDLED) {
						error = B_OK;
printf("%*s  -> child unhandled\n", level * 2, "");
					}

					if (error != B_OK) {
						delete childEntry;
						return error;
					}
				} else
					delete childEntry;
			}

			if (endOfEntryList)
				break;
		}
	}

	entryDeleter.Detach();
	_entry = entry;
	_endOfEntryList = false;
	return B_OK;
}


status_t
DwarfFile::_FinishCompilationUnit(CompilationUnit* unit)
{
printf("\nfinishing compilation unit %p\n", unit);
	AbbreviationTable* abbreviationTable = unit->GetAbbreviationTable();

	DataReader dataReader(
		(const uint8*)fDebugInfoSection->Data() + unit->HeaderOffset(),
		unit->TotalSize());
//	DataReader dataReader(
//		(const uint8*)fDebugInfoSection->Data() + unit->ContentOffset(),
//		unit->ContentSize());

	DebugInfoEntryInitInfo entryInitInfo;

	int entryCount = unit->CountEntries();
	for (int i = 0; i < entryCount; i++) {
		// get the entry
		DebugInfoEntry* entry;
		dwarf_off_t offset;
		unit->GetEntryAt(i, entry, offset);
printf("entry %p at %lu\n", entry, offset);

		// seek the reader to the entry
		dataReader.SeekAbsolute(offset);

		// read the entry code
		uint32 code = dataReader.ReadUnsignedLEB128(0);

		// get the respective abbreviation entry
		AbbreviationEntry abbreviationEntry;
		abbreviationTable->GetAbbreviationEntry(code, abbreviationEntry);

		// initialization before setting the attributes
		status_t error = entry->InitAfterHierarchy(entryInitInfo);
		if (error != B_OK) {
			fprintf(stderr, "Init after hierarchy failed!\n");
			return error;
		}

		// parse the attributes -- this time pass the entry, so that the
		// attribute get set on it
		error = _ParseEntryAttributes(dataReader, entry, abbreviationEntry);
		if (error != B_OK)
			return error;

		// initialization after setting the attributes
		error = entry->InitAfterAttributes(entryInitInfo);
		if (error != B_OK) {
			fprintf(stderr, "Init after attributes failed!\n");
			return error;
		}
	}

	return B_OK;
}


status_t
DwarfFile::_ParseEntryAttributes(DataReader& dataReader,
	DebugInfoEntry* entry, AbbreviationEntry& abbreviationEntry)
{
	uint32 attributeName;
	uint32 attributeForm;
	while (abbreviationEntry.GetNextAttribute(attributeName,
			attributeForm)) {
		// resolve attribute form indirection
		if (attributeForm == DW_FORM_indirect)
			attributeForm = dataReader.ReadUnsignedLEB128(0);

		// prepare an AttributeValue
		AttributeValue attributeValue;
		attributeValue.attributeForm = attributeForm;
		attributeValue.isSigned = false;

		// Read the attribute value according to the attribute's form. For
		// the forms that don't map to a single attribute class only or
		// those that need additional processing, we read a temporary value
		// first.
		uint64 value = 0;
		dwarf_size_t blockLength = 0;
		bool localReference = true;

		switch (attributeForm) {
			case DW_FORM_addr:
				value = dataReader.Read<dwarf_addr_t>(0);
				break;
			case DW_FORM_block2:
				blockLength = dataReader.Read<uint16>(0);
				break;
			case DW_FORM_block4:
				blockLength = dataReader.Read<uint32>(0);
				break;
			case DW_FORM_data2:
				value = dataReader.Read<uint16>(0);
				break;
			case DW_FORM_data4:
				value = dataReader.Read<uint32>(0);
				break;
			case DW_FORM_data8:
				value = dataReader.Read<uint64>(0);
				break;
			case DW_FORM_string:
				attributeValue.string = dataReader.ReadString();
				break;
			case DW_FORM_block:
				blockLength = dataReader.ReadUnsignedLEB128(0);
				break;
			case DW_FORM_block1:
				blockLength = dataReader.Read<uint8>(0);
				break;
			case DW_FORM_data1:
				value = dataReader.Read<uint8>(0);
				break;
			case DW_FORM_flag:
				attributeValue.flag = dataReader.Read<uint8>(0) != 0;
				break;
			case DW_FORM_sdata:
				value = dataReader.ReadSignedLEB128(0);
				attributeValue.isSigned = true;
				break;
			case DW_FORM_strp:
			{
				dwarf_off_t offset = dataReader.Read<dwarf_off_t>(0);
				if (offset >= fDebugStringSection->Size()) {
					fprintf(stderr, "Invalid DW_FORM_strp offset: %lu\n",
						offset);
					return B_BAD_DATA;
				}
				attributeValue.string
					= (const char*)fDebugStringSection->Data() + offset;
				break;
			}
			case DW_FORM_udata:
				value = dataReader.ReadUnsignedLEB128(0);
				break;
			case DW_FORM_ref_addr:
				value = dataReader.Read<dwarf_off_t>(0);
				localReference = false;
				break;
			case DW_FORM_ref1:
				value = dataReader.Read<uint8>(0);
				break;
			case DW_FORM_ref2:
				value = dataReader.Read<uint16>(0);
				break;
			case DW_FORM_ref4:
				value = dataReader.Read<uint32>(0);
				break;
			case DW_FORM_ref8:
				value = dataReader.Read<uint64>(0);
				break;
			case DW_FORM_ref_udata:
				value = dataReader.ReadUnsignedLEB128(0);
				break;
			case DW_FORM_indirect:
			default:
				fprintf(stderr, "Unsupported attribute form: %lu\n",
					attributeForm);
				return B_BAD_DATA;
		}

		// get the attribute class -- skip the attribute, if we can't handle
		// it
		uint8 attributeClass = get_attribute_class(attributeName,
			attributeForm);
		if (attributeClass == ATTRIBUTE_CLASS_UNKNOWN) {
			printf("skipping attribute with unrecognized class: %s (%#lx) "
				"%s (%#lx)\n", get_attribute_name_name(attributeName),
				attributeName, get_attribute_form_name(attributeForm),
				attributeForm);
			continue;
		}
		attributeValue.attributeClass = attributeClass;

		// set the attribute value according to the attribute's class
		switch (attributeClass) {
			case ATTRIBUTE_CLASS_ADDRESS:
				attributeValue.address = value;
				break;
			case ATTRIBUTE_CLASS_BLOCK:
				attributeValue.block.data = dataReader.Data();
				attributeValue.block.length = blockLength;
				dataReader.Skip(blockLength);
				break;
			case ATTRIBUTE_CLASS_CONSTANT:
				attributeValue.constant = value;
				break;
			case ATTRIBUTE_CLASS_LINEPTR:
			case ATTRIBUTE_CLASS_LOCLISTPTR:
			case ATTRIBUTE_CLASS_MACPTR:
			case ATTRIBUTE_CLASS_RANGELISTPTR:
				attributeValue.pointer = value;
				break;
			case ATTRIBUTE_CLASS_REFERENCE:
				if (entry != NULL) {
					attributeValue.reference = _ResolveReference(value,
						localReference);
					if (attributeValue.reference == NULL) {
						fprintf(stderr, "Failed to resolve reference: "
						"%s (%#lx) %s (%#lx): value: %llu\n",
						get_attribute_name_name(attributeName),
						attributeName,
						get_attribute_form_name(attributeForm),
						attributeForm, value);
						return B_ENTRY_NOT_FOUND;
					}
				}
				break;
			case ATTRIBUTE_CLASS_FLAG:
			case ATTRIBUTE_CLASS_STRING:
				// already set
				break;
		}

		if (dataReader.HasOverflow()) {
			fprintf(stderr, "Unexpected end of .debug_info section.\n");
			return B_BAD_DATA;
		}

		// add the attribute
		if (entry != NULL) {
char buffer[1024];
printf("  attr %s %s (%d): %s\n", get_attribute_name_name(attributeName),
get_attribute_form_name(attributeForm), attributeClass, attributeValue.ToString(buffer, sizeof(buffer)));

			DebugInfoEntrySetter attributeSetter
				= get_attribute_name_setter(attributeName);
			if (attributeSetter != NULL) {
				status_t error = (entry->*attributeSetter)(attributeName,
					attributeValue);

				if (error == ATTRIBUTE_NOT_HANDLED) {
					error = B_OK;
printf("    -> unhandled\n");
				}

				if (error != B_OK) {
					fprintf(stderr, "Failed to set attribute: name: %s, "
						"form: %s: %s\n",
						get_attribute_name_name(attributeName),
						get_attribute_form_name(attributeForm),
						strerror(error));
				}
			}
else
printf("    -> no attribute setter!\n");
		}
	}

	return B_OK;
}


status_t
DwarfFile::_GetAbbreviationTable(off_t offset, AbbreviationTable*& _table)
{
	// check, whether we've already loaded it
	for (AbbreviationTableList::Iterator it
				= fAbbreviationTables.GetIterator();
			AbbreviationTable* table = it.Next();) {
		if (offset == table->Offset()) {
			_table = table;
			return B_OK;
		}
	}

	// create a new table
	AbbreviationTable* table = new(std::nothrow) AbbreviationTable(offset);
	if (table == NULL)
		return B_NO_MEMORY;

	status_t error = table->Init(fDebugAbbrevSection->Data(),
		fDebugAbbrevSection->Size());
	if (error != B_OK) {
		delete table;
		return error;
	}

	fAbbreviationTables.Add(table);
	_table = table;
	return B_OK;
}


DebugInfoEntry*
DwarfFile::_ResolveReference(uint64 offset, bool localReference) const
{
	if (localReference)
		return fCurrentCompilationUnit->EntryForOffset(offset);

	// TODO: Implement program-global references!
	return NULL;
}
