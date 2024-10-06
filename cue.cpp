/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * Flacon - audio File Encoder
 * https://github.com/flacon/flacon
 *
 * Copyright: 2012-2015
 *   Alexander Sokoloff <sokoloff.a@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "cue.h"
#include "cuedata.h"
#include "types.h"
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QDebug>
#include "uchardetect.h"
#include "profiles.h"
#include "inputaudiofile.h"
#include "formats_in/informat.h"
#include <QBuffer>

/**************************************
 * Tags::Track
 **************************************/
Tags::Track Cue::Track::decode(const TextCodec &textCodec) const
{
    Tags::Track res;
    // clang-format off
    res.setTrackNum(mTrackNumTag);

    if (!mDateTag.isNull())       res.setDate(textCodec.decode(mDateTag));
    if (!mFlagsTag.isNull())      res.setFlagsTag(textCodec.decode(mFlagsTag));
    if (!mIsrcTag.isNull())       res.setIsrc(textCodec.decode(mIsrcTag));
    if (!mPerformerTag.isNull())  res.setPerformer(textCodec.decode(mPerformerTag));
    if (!mSongWriterTag.isNull()) res.setSongWriter(textCodec.decode(mSongWriterTag));
    if (!mTitleTag.isNull())      res.setTitle(textCodec.decode(mTitleTag));
    // clang-format on

    return res;
}

/************************************************
 *
 ************************************************/
Cue::Cue(const QString &fileName) noexcept(false)
{
    CueData data(fileName);
    if (data.isEmpty()) {
        throw CueError(QObject::tr("<b>%1</b> is not a valid CUE file. The CUE sheet has no FILE tag.").arg(fileName));
    }

    QFileInfo fileInfo(fileName);
    mFilePath     = fileInfo.absoluteFilePath();
    mTagsId.uri   = fileInfo.absoluteFilePath();
    mTagsId.title = fileInfo.fileName();

    read(data);
}

/************************************************
 *
 ************************************************/
void Cue::read(const CueData &data)
{
    mDiscCountTag = 1;
    mDiscNumTag   = 1;

    const CueData::Tags &global = data.globalTags();

    mAlbumTag      = global.value(CueData::TITLE_TAG);
    mCatalogTag    = global.value("CATALOG");
    mCdTextfileTag = global.value("CDTEXTFILE");
    mCommentTag    = global.value("COMMENT");
    mGenreTag      = global.value("GENRE");
    mDiscIdTag     = global.value("DISCID");
    mPerformerTag  = getAlbumPerformer(data);
    mSongWriterTag = global.value("SONGWRITER");
    mDateTag       = global.value("DATE");

    // track.tags[TagId::DiscNum]   = global.value("DISCNUMBER", "1");
    // track.tags[TagId::DiscCount] = global.value("TOTALDISCS", "1");

    QByteArray prevFileTag;

    for (const CueData::Tags &t : data.tracks()) {
        Track track;
        track.mFileTag = t.value(CueData::FILE_TAG);

        QByteArray index00     = t.value("INDEX 00");
        QByteArray index00File = t.value("INDEX 00 FILE");

        QByteArray index01     = t.value("INDEX 01");
        QByteArray index01File = t.value("INDEX 01 FILE");

        if (index00.isEmpty() && !index01.isEmpty()) {
            if (index01File == prevFileTag) {
                index00     = index01;
                index00File = index01File;
            }
            else {
                index00     = "00:00:00";
                index00File = index01File;
            }
        }

        if (index01.isEmpty() && !index00.isEmpty()) {
            index01     = index00;
            index01File = index00File;
        }
        prevFileTag = index01File;

        track.mCueIndex00 = CueIndex(index00, index00File);
        track.mCueIndex01 = CueIndex(index01, index01File);

        track.mFlagsTag      = t.value(CueData::FLAGS_TAG);
        track.mIsrcTag       = t.value("ISRC");
        track.mTitleTag      = t.value("TITLE");
        track.mPerformerTag  = t.value("PERFORMER");
        track.mSongWriterTag = t.value("SONGWRITER");
        track.mDateTag       = t.value("DATE");
        track.mTrackNumTag   = TrackNum(t.value(CueData::TRACK_TAG).toUInt());

        mTracks.append(track);
    }

    if (Profile::isSplitTrackTitle()) {
        splitTitleTag(data);
    }

    {
        mFileTags.clear();
        mMutiplyAudio = false;

        QString prev;
        for (const Track &track : qAsConst(mTracks)) {
            QString s = QString::fromUtf8(track.fileTag());

            if (s != prev) {
                mMutiplyAudio = !mFileTags.isEmpty();
                mFileTags << s;
                prev = s;
            }
        }
    }

    mBom = data.bomCodec();
    validate();
}

/************************************************
 *
 ************************************************/
void Cue::validate()
{
    bool hasFileTag = false;
    for (const Track &t : qAsConst(mTracks)) {
        hasFileTag = hasFileTag || !t.fileTag().isEmpty();
    }

    if (!hasFileTag) {
        throw CueError(QObject::tr("<b>%1</b> is not a valid CUE file. The CUE sheet has no FILE tag.").arg(mFilePath));
    }
}

/************************************************
 *
 ************************************************/
void Cue::splitTitle(Track *track, char separator)
{
    QByteArray b         = track->titleTag();
    int        pos       = b.indexOf(separator);
    track->mPerformerTag = b.left(pos).trimmed();
    track->mTitleTag     = b.right(b.length() - pos - 1).trimmed();
}

/************************************************
 *
 ************************************************/
TextCodec Cue::detectTextCodec() const
{
    UcharDet uchardet;

    uchardet << mPerformerTag;
    uchardet << mAlbumTag;

    for (const Track &track : mTracks) {
        uchardet << track.performerTag();
        uchardet << track.titleTag();
    }

    return uchardet.detect();
}

Tags Cue::decode(const TextCodec &textCodec) const
{
    Tags res;
    // clang-format off
    if (!mAlbumTag.isNull())      res.setAlbum(textCodec.decode(mAlbumTag));
    if (!mCatalogTag.isNull())    res.setCatalog(textCodec.decode(mCatalogTag));
    if (!mCommentTag.isNull())    res.setComment(textCodec.decode(mCommentTag));
    if (!mDateTag.isNull())       res.setDate(textCodec.decode(mDateTag));
    if (!mDiscIdTag.isNull())     res.setDiscId(textCodec.decode(mDiscIdTag));
    if (!mGenreTag.isNull())      res.setGenre(textCodec.decode(mGenreTag));
    if (!mPerformerTag.isNull())  res.setPerformer(textCodec.decode(mPerformerTag));
    if (!mSongWriterTag.isNull()) res.setSongWriter(textCodec.decode(mSongWriterTag));
    // clang-format on

    for (const Track &t : mTracks) {
        res.tracks().append(t.decode(textCodec));
    }

    return res;
}

/************************************************
 *
 ************************************************/
QByteArray Cue::getAlbumPerformer(const CueData &data)
{
    QByteArray res = data.globalTags().value(CueData::PERFORMER_TAG);

    if (!res.isEmpty())
        return res;

    res = data.tracks().first().value(CueData::PERFORMER_TAG);

    for (const CueData::Tags &t : data.tracks()) {
        if (t.value(CueData::PERFORMER_TAG) != res)
            return "";
    }
    return res;
}

/************************************************
 *
 ************************************************/
void Cue::splitTitleTag(const CueData &data)
{
    bool splitByDash      = true;
    bool splitBySlash     = true;
    bool splitByBackSlash = true;

    for (const CueData::Tags &track : data.tracks()) {
        if (track.contains("PERFORMER")) {
            return;
        }

        QByteArray value = track.value("TITLE");
        splitByDash      = splitByDash && value.contains('-');
        splitBySlash     = splitBySlash && value.contains('/');
        splitByBackSlash = splitByBackSlash && value.contains('\\');
    }

    // clang-format off
    for (Track &track : mTracks) {
        if (splitByBackSlash)  splitTitle(&track, '\\');
        else if (splitBySlash) splitTitle(&track, '/');
        else if (splitByDash)  splitTitle(&track, '-');
    }
    // clang-format on
}

/************************************************
 *
 ************************************************/
EmbeddedCue::EmbeddedCue(const InputAudioFile &audioFile) noexcept(false) :
    Cue()
{
    QFileInfo fileInfo(audioFile.filePath());
    mFilePath     = EMBEDED_PREFIX + fileInfo.absoluteFilePath();
    mTagsId.uri   = mFilePath;
    mTagsId.title = QObject::tr("Embedded on %1", "The title for the CUE embedded in the audio file. %1 - is an audio-file name.").arg(fileInfo.fileName());

    if (!audioFile.isValid() || !audioFile.format()) {
        return;
    }

    QByteArray bytes = audioFile.format()->readEmbeddedCue(audioFile.filePath());
    if (bytes.isEmpty()) {
        return;
    }

    QBuffer buf(&bytes);
    buf.open(QBuffer::ReadOnly);

    CueData data(&buf);
    if (data.isEmpty()) {
        return;
    }

    read(data);
}

/**************************************
 *
 **************************************/
CueError::~CueError()
{
}
