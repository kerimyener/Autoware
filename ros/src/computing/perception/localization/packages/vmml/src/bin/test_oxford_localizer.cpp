/*
 * test_localizer.cpp
 *
 *  Created on: Aug 29, 2018
 *      Author: sujiwo
 */



#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <histedit.h>
#include <editline/readline.h>
#include <opencv2/highgui.hpp>

#include <boost/filesystem.hpp>

#include "VMap.h"
#include "ImageDatabase.h"
#include "SequenceSLAM.h"
#include "datasets/OxfordDataset.h"


using namespace std;
namespace fs = boost::filesystem;


typedef vector<string> stringTokens;


const string TestPrompt = "OX>";


class LineEditor
{
public:
LineEditor(const char *argv0, const string &askPrompt="")
{
	prompt = askPrompt + ' ';
	el = el_init("test_localizer", stdin, stdout, stderr);
	rl_initialize();
	tokenizer = tok_init(NULL);
}

~LineEditor()
{
	tok_end(tokenizer);
	el_end(el);
}

stringTokens getLine ()
{
	int nt;
	const char **stoklist;
	char *sline = readline(prompt.c_str());
	tok_str(tokenizer, sline, &nt, &stoklist);

	stringTokens st;
	for (int i=0; i<nt; i++) {
		string s(stoklist[i]);
		st.push_back(s);
	}
	free(sline);
	tok_reset(tokenizer);
	return st;
}

protected:
	EditLine *el;
	Tokenizer *tokenizer;
	string prompt;
};


int localize_seq_slam (SequenceSLAM *seqSl, OxfordImagePreprocessor &proc, const string &imgPath)
{
	cv::Mat image = proc.load(imgPath);
	cv::imwrite("/tmp/1.png", image);
	exit(-1);

	vector<cv::Mat> imgList;
	imgList.push_back(image);

	seqSl->find(imgList, 10);
}


#define Precision 10

string dumpVector(const Vector3d &v)
{
	stringstream s;
	s.precision(Precision);
	s << v.x() << " " << v.y() << " " << v.z();
	return s.str();
}


string dumpVector(const Quaterniond &v)
{
	stringstream s;
	s.precision(Precision);
	s << v.x() << " " << v.y() << " " << v.z() << ' ' << v.w();
	return s.str();
}



class LocalizerApp
{
public:
LocalizerApp (int argc, char *argv[]):
	mLineEditor(argv[0], TestPrompt)
{

}


~LocalizerApp ()
{
	if (mapSrc)
		delete(mapSrc);
	if (localizTestDataSrc)
		delete(localizTestDataSrc);
}


void loop()
{
	bool doExit=false;
	while (doExit==false) {

		stringTokens command = mLineEditor.getLine();

		if (command[0]=="quit")
			doExit = true;

		else if (command[0]=="map")
			map_open_cmd(command[1]);

		else if (command[0]=="dataset")
			dataset_open_cmd(command[1], command[2]);

		else if (command[0]=="dataset_trajectory")
			dataset_trajectory_dump();

		else if (command[0]=="map_trajectory")
			map_trajectory_dump();

		else if (command[0]=="find")
			map_find_cmd(command[1]);

		else if (command[0]=="save")
			dataset_save_dsecond(command[1]);

		else if (command[0]=="zoom")
			dataset_set_zoom(command[1]);
	}
}


protected:
	LineEditor mLineEditor;

	VMap *mapSrc = NULL;
	ImageDatabase *imgDb = NULL;
	SequenceSLAM *seqSlProv = NULL;

	OxfordDataset *localizTestDataSrc = NULL;


private:

	void map_open_cmd(const string &mapPath)
	{
		mapSrc = new VMap();
		mapSrc->load(mapPath);
		debug("Map loaded");
		imgDb = mapSrc->getImageDB();
		seqSlProv = imgDb->getSequence();
	}

	const string dumpMapTrajectoryPath = "/tmp/dump_map_trajectory.csv";
	void map_trajectory_dump()
	{
		fstream mapTrFd (dumpMapTrajectoryPath, ios_base::out|ios_base::trunc);
		if (!mapTrFd.is_open()) {
			debug("Unable to create "+dumpMapTrajectoryPath);
			return;
		}

		auto mapPoses = mapSrc->dumpCameraPoses();
		for (auto ps: mapPoses) {
			mapTrFd << dumpVector(ps.first) << " " << dumpVector(ps.second) << endl;
		}

		mapTrFd.close();
		debug("Map trajectory dumped to "+dumpMapTrajectoryPath);
	}

	const string dumpDatasetTrajectoryPath = "/tmp/dump_dataset_trajectory";
	void dataset_trajectory_dump()
	{
		const string dsDumpPath = dumpDatasetTrajectoryPath + '-' + fs::basename(localizTestDataSrc->getPath());
		fstream dsTrFd (dsDumpPath, ios_base::out|ios_base::trunc);
		if (!dsTrFd.is_open()) {
			debug("Unable to create "+dumpMapTrajectoryPath);
			return;
		}

		for (int i=0; i<localizTestDataSrc->size(); i++) {
			const OxfordDataItem di = localizTestDataSrc->at(i);
			dsTrFd << di.timestamp << " "
					<< dumpVector(di.getPosition()) << " "
					<< dumpVector(di.getOrientation())
					<< endl;
		}

		dsTrFd.close();
		debug("Dataset trajectory dumped to "+dsDumpPath);
	}


	void dataset_open_cmd(const string &dsPath, const string &modelDir)
	{
		localizTestDataSrc = new OxfordDataset(dsPath, modelDir);
		debug("Dataset loaded");
	}

	void map_find_cmd(const string &durationSecStr)
	{
		double d = std::stod(durationSecStr);
		auto di = localizTestDataSrc->atDurationSecond(d);
		cv::Mat img = di.getImage();
		cv::cvtColor(img, img, CV_RGB2GRAY);
		seqSlProv->find(img, 10);
	}

	const string dumpImagePath = "/tmp/dump_image.png";

	void dataset_save_dsecond(const string &durationSecStr)
	{
		double d = std::stod(durationSecStr);
		auto di = localizTestDataSrc->atDurationSecond(d);
		cv::Mat img = di.getImage();
		cv::imwrite(dumpImagePath, img);
		debug("Dumped to " + dumpImagePath);
	}

	void debug(const string &s)
	{
		cerr << s << endl;
	}

	void dataset_set_zoom(const string &zstr)
	{
		float z = std::stof(zstr);
		localizTestDataSrc->setZoomRatio(z);
	}

};





int main (int argc, char *argv[])
{
	LocalizerApp mainApp(argc, argv);
	mainApp.loop();

	return 0;
}
