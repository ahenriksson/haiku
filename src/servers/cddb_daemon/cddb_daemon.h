/*
 * Copyright 2008, Bruno Albuquerque, bga@bug-br.org.br. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef CDDB_DAEMON_H
#define CDDB_DAEMON_H

#include <Application.h>
#include <Message.h>
#include <VolumeRoster.h>

#include <scsi_cmds.h>

class CDDBDaemon : public BApplication
{
public:
	CDDBDaemon();
	virtual ~CDDBDaemon();

	virtual void MessageReceived(BMessage* message);

private:
	bool CanLookup(const dev_t _device, uint32* _cddbId,
		scsi_toc_toc* _toc) const;

	BVolumeRoster* fVolumeRoster;
};

#endif
