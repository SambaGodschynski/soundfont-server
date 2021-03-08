//=============================================================================
//  MuseScore
//  Linux Music Score Editor
//  $Id:$
//
//  Copyright (C) 2010 Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "sfont.h"
#include "time.h"


using namespace SfTools;


#define XDEBUG(x)

#define BE_SHORT(x) ((((x)&0xFF)<<8) | (((x)>>8)&0xFF))
#ifdef __i486__
#define BE_LONG(x) \
     ({ int __value; \
        asm ("bswap %1; movl %1,%0" : "=g" (__value) : "r" (x)); \
       __value; })
#else
#define BE_LONG(x) ((((x)&0xFF)<<24) | \
              (((x)&0xFF00)<<8) | \
              (((x)&0xFF0000)>>8) | \
              (((x)>>24)&0xFF))
#endif

#define FOURCC(a, b, c, d) a << 24 | b << 16 | c << 8 | d

#define BLOCK_SIZE 1024

static const bool writeCompressed = true;



//---------------------------------------------------------
//   Sample
//---------------------------------------------------------

Sample::Sample()
{
	name = nullptr;
	start = 0;
	end = 0;
	loopstart = 0;
	loopend = 0;
	samplerate = 0;
	origpitch = 0;
	pitchadj = 0;
	sampleLink = 0;
	sampletype = 0;
}

Sample::~Sample()
{
	free(name);
}

//---------------------------------------------------------
//   Instrument
//---------------------------------------------------------

Instrument::Instrument()
{
	name = nullptr;
}

Instrument::~Instrument()
{
	free(name);
	for (auto x : this->zones) {
		delete x;
	}
}

//---------------------------------------------------------
//   SoundFont
//---------------------------------------------------------

SoundFont::SoundFont(const QString& s)
{
	path = s;
	engine = nullptr;
	name = nullptr;
	date = nullptr;
	comment = nullptr;
	tools = nullptr;
	creator = nullptr;
	product = nullptr;
	copyright = nullptr;
	irom = nullptr;
	version.major = 0;
	version.minor = 0;
	iver.major = 0;
	iver.minor = 0;
	_smallSf = false;
	using namespace std::placeholders;
	readSampleFunction = std::bind(&SoundFont::readSample, this, _1, _2, _3);
}

SoundFont::~SoundFont()
{
	free(engine);
	free(name);
	free(date);
	free(comment);
	free(tools);
	free(creator);
	free(product);
	free(copyright);
	free(irom);

	for (auto x : presets) {
		delete x;
	}
	for (auto x : instruments) {
		delete x;
	}
	for (auto x : samples) {
		delete x;
	}
}
//---------------------------------------------------------
//   read
//---------------------------------------------------------

bool SoundFont::read()
{
	file = new QFile(path);
	if (!file->open(QIODevice::ReadOnly)) {
		throw std::runtime_error("could not open: " + path);
		delete file;
		return false;
	}
	try {
		int len = readFourcc("RIFF");
		readSignature("sfbk");
		len -= 4;
		while (len) {
			int len2 = readFourcc("LIST");
			len -= (len2 + 8);
			char fourcc[5];
			fourcc[0] = 0;
			readSignature(fourcc);
			fourcc[4] = 0;
			len2 -= 4;
			while (len2) {
				fourcc[0] = 0;
				int len3 = readFourcc(fourcc);
				fourcc[4] = 0;
				len2 -= (len3 + 8);
				readSection(fourcc, len3);
			}
		}
	}
	catch (...) {
		delete file;
		throw;
	}
	delete file;
	return true;
}

//---------------------------------------------------------
//   skip
//---------------------------------------------------------

void SoundFont::skip(int n)
{
	qint64 pos = file->pos();
	if (!file->seek(pos + n))
		throw std::runtime_error("unexpected end of file");
}


//---------------------------------------------------------
//   readFourcc
//---------------------------------------------------------

int SoundFont::readFourcc(char* signature)
{
	readSignature(signature);
	return readDword();
}

int SoundFont::readFourcc(const char* signature)
{
	readSignature(signature);
	return readDword();
}

//---------------------------------------------------------
//   readSignature
//---------------------------------------------------------

void SoundFont::readSignature(const char* signature)
{
	char fourcc[4];
	readSignature(fourcc);
	if (memcmp(fourcc, signature, 4) != 0)
		throw std::runtime_error("fourcc expected " + std::string(signature));
}

void SoundFont::readSignature(char* signature)
{
	if (file->read(signature, 4) != 4)
		throw std::runtime_error("unexpected end of file");
}

//---------------------------------------------------------
//   readDword
//---------------------------------------------------------

unsigned SoundFont::readDword()
{
	unsigned format;
	if (file->read((char*)&format, 4) != 4)
		throw std::runtime_error("unexpected end of file");
	if (QSysInfo::ByteOrder == QSysInfo::BigEndian)
		return BE_LONG(format);
	else
		return format;
}



//---------------------------------------------------------
//   writeDword
//---------------------------------------------------------

void SoundFont::writeDword(int val)
{
	if (QSysInfo::ByteOrder == QSysInfo::BigEndian)
		val = BE_LONG(val);
	write((char*)&val, 4);
}

//---------------------------------------------------------
//   writeWord
//---------------------------------------------------------

void SoundFont::writeWord(unsigned short int val)
{
	if (QSysInfo::ByteOrder == QSysInfo::BigEndian)
		val = BE_SHORT(val);
	write((char*)&val, 2);
}

//---------------------------------------------------------
//   writeByte
//---------------------------------------------------------

void SoundFont::writeByte(unsigned char val)
{
	write((char*)&val, 1);
}

//---------------------------------------------------------
//   writeChar
//---------------------------------------------------------

void SoundFont::writeChar(char val)
{
	write((char*)&val, 1);
}

//---------------------------------------------------------
//   writeShort
//---------------------------------------------------------

void SoundFont::writeShort(short val)
{
	if (QSysInfo::ByteOrder == QSysInfo::BigEndian)
		val = BE_SHORT(val);
	write((char*)&val, 2);
}

//---------------------------------------------------------
//   readWord
//---------------------------------------------------------

int SoundFont::readWord()
{
	unsigned short format;
	if (file->read((char*)&format, 2) != 2)
		throw std::runtime_error("unexpected end of file");
	if (QSysInfo::ByteOrder == QSysInfo::BigEndian)
		return BE_SHORT(format);
	else
		return format;
}

//---------------------------------------------------------
//   readShort
//---------------------------------------------------------

int SoundFont::readShort()
{
	short format;
	if (file->read((char*)&format, 2) != 2)
		throw std::runtime_error("unexpected end of file");
	if (QSysInfo::ByteOrder == QSysInfo::BigEndian)
		return BE_SHORT(format);
	else
		return format;
}

//---------------------------------------------------------
//   readByte
//---------------------------------------------------------

int SoundFont::readByte()
{
	uchar val;
	if (file->read((char*)&val, 1) != 1)
		throw std::runtime_error("unexpected end of file");
	return val;
}

//---------------------------------------------------------
//   readChar
//---------------------------------------------------------

int SoundFont::readChar()
{
	char val;
	if (file->read(&val, 1) != 1)
		throw std::runtime_error("unexpected end of file");
	return val;
}

//---------------------------------------------------------
//   readVersion
//---------------------------------------------------------

void SoundFont::readVersion(sfVersionTag* v)
{
	unsigned char data[4];
	if (file->read((char*)data, 4) != 4)
		throw std::runtime_error("unexpected end of file");
	v->major = data[0] + (data[1] << 8);
	v->minor = data[2] + (data[3] << 8);
}

//---------------------------------------------------------
//   readString
//---------------------------------------------------------

char* SoundFont::readString(int n)
{
	char data[2500];
	if (file->read((char*)data, n) != n)
		throw std::runtime_error("unexpected end of file");
	if (data[n - 1] != 0)
		data[n] = 0;
	return strdup(data);
}

//---------------------------------------------------------
//   readSection
//---------------------------------------------------------

void SoundFont::readSection(const char* fourcc, int len)
{
	XDEBUG(printf("readSection <%s> len %d\n", fourcc, len);)

		switch (FOURCC(fourcc[0], fourcc[1], fourcc[2], fourcc[3])) {
		case FOURCC('i', 'f', 'i', 'l'):    // version
			readVersion(&version);
			break;
		case FOURCC('I', 'N', 'A', 'M'):       // sound font name
			name = readString(len);
			break;
		case FOURCC('i', 's', 'n', 'g'):       // target render engine
			engine = readString(len);
			break;
		case FOURCC('I', 'P', 'R', 'D'):       // product for which the bank was intended
			product = readString(len);
			break;
		case FOURCC('I', 'E', 'N', 'G'): // sound designers and engineers for the bank
			creator = readString(len);
			break;
		case FOURCC('I', 'S', 'F', 'T'): // SoundFont tools used to create and alter the bank
			tools = readString(len);
			break;
		case FOURCC('I', 'C', 'R', 'D'): // date of creation of the bank
			date = readString(len);
			break;
		case FOURCC('I', 'C', 'M', 'T'): // comments on the bank
			comment = readString(len);
			break;
		case FOURCC('I', 'C', 'O', 'P'): // copyright message
			copyright = readString(len);
			break;
		case FOURCC('s', 'm', 'p', 'l'): // the digital audio samples
			samplePos = file->pos();
			sampleLen = len;
			skip(len);
			break;
		case FOURCC('s', 'm', '2', '4'): // audio samples (24-bit part)
			skip(len); // Just skip it
			break;
		case FOURCC('p', 'h', 'd', 'r'): // preset headers
			readPhdr(len);
			break;
		case FOURCC('p', 'b', 'a', 'g'): // preset index list
			readBag(len, &pZones);
			break;
		case FOURCC('p', 'm', 'o', 'd'): // preset modulator list
			readMod(len, &pZones);
			break;
		case FOURCC('p', 'g', 'e', 'n'): // preset generator list
			readGen(len, &pZones);
			break;
		case FOURCC('i', 'n', 's', 't'): // instrument names and indices
			readInst(len);
			break;
		case FOURCC('i', 'b', 'a', 'g'): // instrument index list
			readBag(len, &iZones);
			break;
		case FOURCC('i', 'm', 'o', 'd'): // instrument modulator list
			readMod(len, &iZones);
			break;
		case FOURCC('i', 'g', 'e', 'n'): // instrument generator list
			readGen(len, &iZones);
			break;
		case FOURCC('s', 'h', 'd', 'r'): // sample headers
			readShdr(len);
			break;
		case FOURCC('i', 'r', 'o', 'm'):    // sample rom
			irom = readString(len);
			break;
		case FOURCC('i', 'v', 'e', 'r'):    // sample rom version
			readVersion(&iver);
			break;
		default:
			skip(len);
			throw std::runtime_error("unknown fourcc " + std::string(fourcc));
		}
}

//---------------------------------------------------------
//   readPhdr
//---------------------------------------------------------

void SoundFont::readPhdr(int len)
{
	if (len < (38 * 2))
		throw std::runtime_error("phdr too short");
	if (len % 38)
		throw std::runtime_error("phdr not a multiple of 38");
	int n = len / 38;
	if (n <= 1) {
		XDEBUG(printf("no presets\n");)
			skip(len);
		return;
	}
	int index1 = 0, index2;
	for (int i = 0; i < n; ++i) {
		Preset* preset = new Preset;
		preset->name = readString(20);
		preset->preset = readWord();
		preset->bank = readWord();
		index2 = readWord();
		preset->library = readDword();
		preset->genre = readDword();
		preset->morphology = readDword();
		if (index2 < index1)
			throw std::runtime_error("preset header indices not monotonic");
		if (i > 0) {
			int n = index2 - index1;
			while (n--) {
				Zone* z = new Zone;
				presets.back()->zones.append(z);
				pZones.append(z);
			}
		}
		index1 = index2;
		presets.append(preset);
	}
	auto *last = presets.takeLast();
	delete last;
}

//---------------------------------------------------------
//   readBag
//---------------------------------------------------------

void SoundFont::readBag(int len, QList<Zone*>* zones)
{
	if (len % 4)
		throw std::runtime_error("bag size not a multiple of 4");
	int gIndex2, mIndex2;
	int gIndex1 = readWord();
	int mIndex1 = readWord();
	len -= 4;
	for (Zone* zone : *zones) {
		gIndex2 = readWord();
		mIndex2 = readWord();
		len -= 4;
		if (len < 0)
			throw(QString("bag size too small"));
		if (gIndex2 < gIndex1)
			throw("generator indices not monotonic");
		if (mIndex2 < mIndex1)
			throw("modulator indices not monotonic");
		int n = mIndex2 - mIndex1;
		while (n--)
			zone->modulators.append(new ModulatorList);
		n = gIndex2 - gIndex1;
		while (n--)
			zone->generators.append(new GeneratorList);
		gIndex1 = gIndex2;
		mIndex1 = mIndex2;
	}
}

//---------------------------------------------------------
//   readMod
//---------------------------------------------------------

void SoundFont::readMod(int size, QList<Zone*>* zones)
{
	for (Zone* zone : *zones) {
		for (ModulatorList* m : zone->modulators) {
			size -= 10;
			if (size < 0)
				throw std::runtime_error("pmod size mismatch");
			m->src = static_cast<Modulator>(readWord());
			m->dst = static_cast<Generator>(readWord());
			m->amount = readShort();
			m->amtSrc = static_cast<Modulator>(readWord());
			m->transform = static_cast<Transform>(readWord());
		}
	}
	if (size != 10)
		throw std::runtime_error("modulator list size mismatch");
	skip(10);
}

//---------------------------------------------------------
//   readGen
//---------------------------------------------------------

void SoundFont::readGen(int size, QList<Zone*>* zones)
{
	if (size % 4)
		throw std::runtime_error("bad generator list size");
	for (Zone* zone : *zones) {
		size -= (zone->generators.size() * 4);
		if (size < 0)
			break;

		for (GeneratorList* gen : zone->generators) {
			gen->gen = static_cast<Generator>(readWord());
			if (gen->gen == Gen_KeyRange || gen->gen == Gen_VelRange) {
				gen->amount.lo = readByte();
				gen->amount.hi = readByte();
			}
			else if (gen->gen == Gen_Instrument)
				gen->amount.uword = readWord();
			else
				gen->amount.sword = readWord();
		}
	}
	if (size != 4)
		throw std::runtime_error("generator list size mismatch" + std::to_string(size) + " != 4");
	skip(size);
}

//---------------------------------------------------------
//   readInst
//---------------------------------------------------------

void SoundFont::readInst(int size)
{
	int n = size / 22;
	int index1 = 0, index2;
	for (int i = 0; i < n; ++i) {
		Instrument* instrument = new Instrument;
		instrument->name = readString(20);
		index2 = readWord();
		if (index2 < index1)
			throw std::runtime_error("instrument header indices not monotonic");
		if (i > 0) {
			int n = index2 - index1;
			while (n--) {
				Zone* z = new Zone;
				instruments.back()->zones.append(z);
				iZones.append(z);
			}
		}
		index1 = index2;
		instruments.append(instrument);
	}
	auto *last = instruments.takeLast();
	delete last;
}

//---------------------------------------------------------
//   readShdr
//---------------------------------------------------------

void SoundFont::readShdr(int size)
{
	int n = size / 46;
	for (int i = 0; i < n - 1; ++i) {
		Sample* s = new Sample;
		s->name = readString(20);
		if (std::string(s->name) == "Rhodes C2(L)") {
			int halt = 0;
		}
		s->start = readDword();
		s->end = readDword();
		s->loopstart = readDword() - s->start;
		s->loopend = readDword() - s->start;
		s->samplerate = readDword();
		s->origpitch = readByte();

		s->pitchadj = readChar();
		s->sampleLink = readWord();
		s->sampletype = readWord();
		samples.append(s);
	}
	skip(46);   // trailing record
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

bool SoundFont::write()
{
	qint64 riffLenPos;
	qint64 listLenPos;
	try {
		file->write("RIFF", 4);
		riffLenPos = file->pos();
		writeDword(0);
		file->write("sfbk", 4);

		file->write("LIST", 4);
		listLenPos = file->pos();
		writeDword(0);
		file->write("INFO", 4);

		writeIfil();
		if (name)
			writeStringSection("INAM", name);
		if (engine)
			writeStringSection("isng", engine);
		if (product)
			writeStringSection("IPRD", product);
		if (creator)
			writeStringSection("IENG", creator);
		if (tools)
			writeStringSection("ISFT", tools);
		if (date)
			writeStringSection("ICRD", date);
		if (comment)
			writeStringSection("ICMT", comment);
		if (copyright)
			writeStringSection("ICOP", copyright);
		if (irom)
			writeStringSection("irom", irom);
		writeIver();

		qint64 pos = file->pos();
		file->seek(listLenPos);
		writeDword(pos - listLenPos - 4);
		file->seek(pos);

		file->write("LIST", 4);
		listLenPos = file->pos();
		writeDword(0);
		file->write("sdta", 4);
		writeSmpl();
		pos = file->pos();
		file->seek(listLenPos);
		writeDword(pos - listLenPos - 4);
		file->seek(pos);

		file->write("LIST", 4);
		listLenPos = file->pos();
		writeDword(0);
		file->write("pdta", 4);

		writePhdr();
		writeBag("pbag", &pZones);
		writeMod("pmod", &pZones);
		writeGen("pgen", &pZones);
		writeInst();
		writeBag("ibag", &iZones);
		writeMod("imod", &iZones);
		writeGen("igen", &iZones);
		writeShdr();

		pos = file->pos();
		file->seek(listLenPos);
		writeDword(pos - listLenPos - 4);
		file->seek(pos);

		qint64 endPos = file->pos();
		file->seek(riffLenPos);
		writeDword(endPos - riffLenPos - 4);
	}
	catch (QString s) {
		throw std::runtime_error("write sf file failed: " + s);
	}
	return true;
}


//---------------------------------------------------------
//   write
//---------------------------------------------------------

void SoundFont::write(const char* p, int n)
{
	if (file->write(p, n) != n)
		throw std::runtime_error("write error");
}

//---------------------------------------------------------
//   writeStringSection
//---------------------------------------------------------

void SoundFont::writeStringSection(const char* fourcc, char* s)
{
	write(fourcc, 4);
	int nn = strlen(s) + 1;
	int n = ((nn + 1) / 2) * 2;
	writeDword(n);
	write(s, nn);
	if (n - nn) {
		char c = 0;
		write(&c, 1);
	}
}

//---------------------------------------------------------
//   writeIfil
//---------------------------------------------------------

void SoundFont::writeIfil()
{
	write("ifil", 4);
	writeDword(4);
	unsigned char data[4];
	version.major = 2;
	version.minor = 1;
	data[0] = version.major;
	data[1] = version.major >> 8;
	data[2] = version.minor;
	data[3] = version.minor >> 8;
	write((char*)data, 4);
}

//---------------------------------------------------------
//   writeIVer
//---------------------------------------------------------

void SoundFont::writeIver()
{
	write("iver", 4);
	writeDword(4);
	unsigned char data[4];
	data[0] = iver.major;
	data[1] = iver.major >> 8;
	data[2] = iver.minor;
	data[3] = iver.minor >> 8;
	write((char*)data, 4);
}

//---------------------------------------------------------
//   writeSmpl
//---------------------------------------------------------

void SoundFont::writeSmpl()
{
	write("smpl", 4);

	qint64 pos = file->pos();
	writeDword(0);
	int currentSamplePos = 0;

	for (Sample* s : samples) {
		int len = copySample(s); // In byte
		s->start = currentSamplePos;
		currentSamplePos += len;
		s->end = currentSamplePos;
		s->loopstart = s->start + s->loopstart;
		s->loopend = s->start + s->loopend;
	}

	qint64 npos = file->pos();
	file->seek(pos);
	writeDword(npos - pos - 4);
	file->seek(npos);
}

//---------------------------------------------------------
//   writePhdr
//---------------------------------------------------------

void SoundFont::writePhdr()
{
	write("phdr", 4);
	int n = presets.size();
	writeDword((n + 1) * 38);
	int zoneIdx = 0;
	for (const Preset* p : presets) {
		writePreset(zoneIdx, p);
		zoneIdx += p->zones.size();
	}
	Preset p;
	writePreset(zoneIdx, &p);
}

//---------------------------------------------------------
//   writePreset
//---------------------------------------------------------

void SoundFont::writePreset(int zoneIdx, const Preset* preset)
{
	char name[20];
	memset(name, 0, 20);
	if (preset->name)
		memcpy(name, preset->name, strlen(preset->name));
	write(name, 20);
	writeWord(preset->preset);
	writeWord(preset->bank);
	writeWord(zoneIdx);
	writeDword(preset->library);
	writeDword(preset->genre);
	writeDword(preset->morphology);
}

//---------------------------------------------------------
//   writeBag
//---------------------------------------------------------

void SoundFont::writeBag(const char* fourcc, QList<Zone*>* zones)
{
	write(fourcc, 4);
	int n = zones->size();
	writeDword((n + 1) * 4);
	int gIndex = 0;
	int pIndex = 0;
	for (const Zone* z : *zones) {
		writeWord(gIndex);
		writeWord(pIndex);
		gIndex += z->generators.size();
		pIndex += z->modulators.size();
	}
	writeWord(gIndex);
	writeWord(pIndex);
}

//---------------------------------------------------------
//   writeMod
//---------------------------------------------------------

void SoundFont::writeMod(const char* fourcc, const QList<Zone*>* zones)
{
	write(fourcc, 4);
	int n = 0;
	for (const Zone* z : *zones)
		n += z->modulators.size();
	writeDword((n + 1) * 10);

	for (const Zone* zone : *zones) {
		for (const ModulatorList* m : zone->modulators)
			writeModulator(m);
	}
	ModulatorList mod;
	memset(&mod, 0, sizeof(mod));
	writeModulator(&mod);
}

//---------------------------------------------------------
//   writeModulator
//---------------------------------------------------------

void SoundFont::writeModulator(const ModulatorList* m)
{
	writeWord(m->src);
	writeWord(m->dst);
	writeShort(m->amount);
	writeWord(m->amtSrc);
	writeWord(m->transform);
}

//---------------------------------------------------------
//   writeGen
//---------------------------------------------------------

void SoundFont::writeGen(const char* fourcc, QList<Zone*>* zones)
{
	write(fourcc, 4);
	int n = 0;
	for (const Zone* z : *zones)
		n += z->generators.size();
	writeDword((n + 1) * 4);

	for (const Zone* zone : *zones) {
		for (const GeneratorList* g : zone->generators)
			writeGenerator(g);
	}
	GeneratorList gen;
	memset(&gen, 0, sizeof(gen));
	writeGenerator(&gen);
}

//---------------------------------------------------------
//   writeGenerator
//---------------------------------------------------------

void SoundFont::writeGenerator(const GeneratorList* g)
{
	writeWord(g->gen);
	if (g->gen == Gen_KeyRange || g->gen == Gen_VelRange) {
		writeByte(g->amount.lo);
		writeByte(g->amount.hi);
	}
	else if (g->gen == Gen_Instrument)
		writeWord(g->amount.uword);
	else
		writeWord(g->amount.sword);
}

//---------------------------------------------------------
//   writeInst
//---------------------------------------------------------

void SoundFont::writeInst()
{
	write("inst", 4);
	int n = instruments.size();
	writeDword((n + 1) * 22);
	int zoneIdx = 0;
	for (const Instrument* p : instruments) {
		writeInstrument(zoneIdx, p);
		zoneIdx += p->zones.size();
	}
	Instrument p;
	writeInstrument(zoneIdx, &p);
}

//---------------------------------------------------------
//   writeInstrument
//---------------------------------------------------------

void SoundFont::writeInstrument(int zoneIdx, const Instrument* instrument)
{
	char name[20];
	memset(name, 0, 20);
	if (instrument->name)
		memcpy(name, instrument->name, strlen(instrument->name));
	write(name, 20);
	writeWord(zoneIdx);
}

//---------------------------------------------------------
//   writeShdr
//---------------------------------------------------------

void SoundFont::writeShdr()
{
	write("shdr", 4);
	writeDword(46 * (samples.size() + 1));
	for (const Sample* s : samples)
		writeSample(s);
	Sample s;
	writeSample(&s);
}

//---------------------------------------------------------
//   writeSample
//---------------------------------------------------------

void SoundFont::writeSample(const Sample* s)
{
	char name[20];
	memset(name, 0, 20);
	if (s->name)
		memcpy(name, s->name, strlen(s->name));
	write(name, 20);
	writeDword(s->start);
	writeDword(s->end);
	writeDword(s->loopstart);
	writeDword(s->loopend);
	writeDword(s->samplerate);
	writeByte(s->origpitch);
	writeChar(s->pitchadj);
	writeWord(s->sampleLink);
	writeWord(s->sampletype);
}


//---------------------------------------------------------
//   checkInstrument
//---------------------------------------------------------

static bool checkInstrument(QList<int> pnums, QList<Preset*> presets, int instrIdx)
{
	foreach(int idx, pnums) {
		Preset* p = presets[idx];
		foreach(Zone * z, p->zones) {
			foreach(GeneratorList * g, z->generators) {
				if (g->gen == Gen_Instrument) {
					if (instrIdx == g->amount.uword)
						return true;
				}
			}
		}
	}
	return false;
}


static bool checkInstrument(QList<Preset*> presets, int instrIdx) {
	for (int i = 0; i < presets.size(); i++) {
		Preset* p = presets[i];
		Zone* z = p->zones[0];
		foreach(GeneratorList * g, z->generators) {
			if (g->gen == Gen_Instrument) {
				if (instrIdx == g->amount.uword)
					return true;
			}
		}
	}
	return false;
}


//---------------------------------------------------------
//   checkSample
//---------------------------------------------------------

static bool checkSample(QList<int> pnums, QList<Preset*> presets, QList<Instrument*> instruments,
	int sampleIdx)
{
	int idx = 0;
	foreach(Instrument * instrument, instruments) {
		if (!checkInstrument(pnums, presets, idx)) {
			++idx;
			continue;
		}
		foreach(Zone * z, instrument->zones) {
			QList<GeneratorList*> gl;
			foreach(GeneratorList * g, z->generators) {
				if (g->gen == Gen_SampleId) {
					if (sampleIdx == g->amount.uword)
						return true;
				}
			}
		}
		++idx;
	}
	return false;
}

//---------------------------------------------------------
//   checkSample
//---------------------------------------------------------

static bool checkSample(QList<Preset*> presets, QList<Instrument*> instruments,
	int sampleIdx)
{
	int idx = 0;
	foreach(Instrument * instrument, instruments) {
		if (!checkInstrument(presets, idx)) {
			++idx;
			continue;
		}
		foreach(Zone * z, instrument->zones) {
			QList<GeneratorList*> gl;
			foreach(GeneratorList * g, z->generators) {
				if (g->gen == Gen_SampleId) {
					if (sampleIdx == g->amount.uword)
						return true;
				}
			}
		}
		++idx;
	}
	return false;
}

void SoundFont::readSample(Sample* s, short* outBuffer, int length) 
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) {
		throw std::runtime_error("cannot open " + f.fileName());
	}
	f.seek(samplePos + s->start * sizeof(short));
	f.read((char*)outBuffer, length * sizeof(short));
	f.close();
}

//---------------------------------------------------------
//   copySample
//---------------------------------------------------------

int SoundFont::copySample(Sample* s)
{
	// Prepare input data
	if (s->end <= s->start) {
		return 0; // TODO: skip it for now
		throw std::runtime_error("invalid sample start and end values");
	}
	int length = s->end - s->start;
	short* ibuffer = new short[length];
	readSampleFunction(s, ibuffer, length);
	file->write((const char*)ibuffer, length * sizeof(short));
	delete[] ibuffer;
	return length;
}