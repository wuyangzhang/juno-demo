/*
    Author: Wuyang Zhang
    June 17.2015
 
*/

#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <sys/time.h>
#include "ImgMatch.h"
int deb = 0;
string ImgMatch::add_DB;  //database address
int ImgMatch::size_DB;    //database size, number of img
string ImgMatch::indexImgAdd; // index-img hash table address
Mat ImgMatch::despDB; // descriptors of src and db img
Mat ImgMatch::dbImg;
Mat ImgMatch::featureCluster;
vector<KeyPoint> ImgMatch::dbKeyPoints;
string ImgMatch::featureClusterAdd;
string ImgMatch::imgInfoAdd;
vector<int> ImgMatch::index_IMG;
vector<int> dscCnt_IMG;
int ImgMatch::minHessian = 1000;
int lastIndex = 0;
int imgCount = 0;
//Changed here
flann::Index ImgMatch::flannIndex;


//till here
ImgMatch::ImgMatch()
{
    minHessian = 1000;
 
}
 
ImgMatch::~ImgMatch()
{
}
//get all descriptor of images in database, save index_img to file;
void ImgMatch::init_DB(int size_DB,string add_DB, string indexImgAdd,string featureClusterAdd){

    printf("\nStart calculating the descriptors\n");

    set_dbSize(size_DB);
    SurfFeatureDetector extractor;
    SurfFeatureDetector detector(minHessian);
    /* loop calculate descriptors of iamges in databas */
    for (int i = 1; i <= size_DB; i++){
        char  imgName[100];     
        sprintf(imgName, "%d.jpg", i);
        string fileName;
        string fileAdd = add_DB;
        fileName = fileAdd.append( string(imgName));
        dbImg = imread(fileName);
        cout<<"File in process: "<<fileName<<"\n";

        if (!dbImg.data){
            cout << "Can NOT find img in database!" << endl;
            return;
        }

        detector.detect(dbImg, dbKeyPoints);
        extractor.compute(dbImg, dbKeyPoints, despDB);

    /* put all descripotrs into featureCLuster */

        featureCluster.push_back(despDB);
 
    /* descriptor index -> image index */
        for (int j = 0; j < despDB.rows; j++){
            index_IMG.push_back(i);
        }
         
    }
     
    /*
        \save index_IMG to file
    */
    ofstream outFile;
    outFile.open(indexImgAdd);
    for (auto &t : index_IMG){
        outFile << t << "\n";

    }
    
    for (auto &t : index_IMG){
        if(t!=lastIndex){
            if ((t = lastIndex + 1)) {    
                if(imgCount!=0){
                    printf("Count for %d is %d \n",imgCount,dscCnt_IMG.at(imgCount-1));
                }
                dscCnt_IMG.push_back(1);
                lastIndex++;
                imgCount++;
            }
            else{
                dscCnt_IMG.push_back(0);
                dscCnt_IMG.push_back(1);
                lastIndex=lastIndex + 2;
                imgCount=imgCount + 2;
            }    
        }
        else{
            dscCnt_IMG.at(t-1)=dscCnt_IMG.at(t-1)+1;
        }
    }
    outFile.close();
    ofstream outFile2;
    outFile2.open("indexDscNo");
    for (auto &t : dscCnt_IMG){
        outFile2 << t << "\n";
    }
    /*
        \save feature cluster into file
    */
    cv::FileStorage storage(featureClusterAdd, cv::FileStorage::WRITE);
    storage << "index" << featureCluster;
    storage.release();
    printf("Finished.\n");
}
 
void init_infoDB(string add_DB){
    const int infoDB_Size = 20;
    string infoDB[infoDB_Size] = { "winlab logo", "microware", "file extinguisher", "winlab door", "winlab inner door", "sofa", "working desk", "trash box", "orbit machine", "car", "computer display" };
 
    for (uint i = 0; i < sizeof(infoDB) / sizeof(*infoDB); i++){
        if (infoDB[i] != "\0"){
            char filename[100];
            sprintf(filename, "INFO_%d.txt", i);
 
            string fileName;
            string fileAdd = add_DB;
            fileName = fileAdd.append(string(filename));
 
            ofstream outFile;
            outFile.open(filename);
            outFile << infoDB[i];
        }
    }
}
 
/*
    \read all descriptors of database img, and descripotr index-img index map hashtable
*/
void ImgMatch:: init_matchImg(string indexImgAdd, string featureClusterAdd,string imgInfoAdd){
    struct timeval tpstart,tpend;
    double timeuse;
    gettimeofday(&tpstart,NULL);
    ifstream inFile(indexImgAdd);
    printf("\nStart loading the database\n");
    int t;
    while (inFile >> t){
        index_IMG.push_back(t);
    }
    
    ifstream inFile2("indexDscNo");
    while (inFile2 >> t){
        dscCnt_IMG.push_back(t);
        if (deb) printf("read : %d \n",t);
    }

    FileStorage storage(featureClusterAdd, FileStorage::READ);
    storage["index"] >> featureCluster;
    storage.release();
    imgInfoAdd = imgInfoAdd;
    printf("Finished\n");
    //time after loading database
    gettimeofday(&tpend,NULL);
    timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+tpend.tv_usec-tpstart.tv_usec;// notice, should include both s and us
    printf("loading database:%fms\n",timeuse / 1000);

    gettimeofday(&tpstart,NULL);

    /* build KDtree */
    flannIndex.build(featureCluster, flann::KMeansIndexParams(), cvflann::FLANN_DIST_EUCLIDEAN);


    //time after building tree
    gettimeofday(&tpend,NULL);
    timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+tpend.tv_usec-tpstart.tv_usec;// notice, should include both s and us
    // printf("used time:%fus\n",timeuse);
    printf("Building Tree:%fms\n",timeuse / 1000); 
}
 
void ImgMatch::matchImg(string srcImgAdd){
    struct timeval tpstart,tpend;
    double timeuse;
    gettimeofday(&tpstart,NULL);

    /* default matched index is 0, means no matching image is found */
    matchedImgIndex = 0;

    srcImg = imread(srcImgAdd);
    if (!srcImg.data){
        cout << "srcImg NOT found" << endl;
        return;
    }

     /* calculate input image's descriptor */
    if(deb)
      printf("read source image success! start calculate descriptor of source image\n");

    SurfFeatureDetector detector(minHessian);
    SurfDescriptorExtractor extractor;
    detector.detect(srcImg, keyPoints1);
    extractor.compute(srcImg, keyPoints1, despSRC);

    if(deb)
      printf("start match source image\n");
    
    /* check whether it can extract descriptors from source image, if it is empty then exits */
    if(despSRC.empty()){
      printf("Error: none descriptors can be extracted from source image");
      return;    
    }

    /* calculate time of extracting descriptors */
    gettimeofday(&tpend,NULL);
    timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+tpend.tv_usec-tpstart.tv_usec;
    printf("descriptor isolation:%fms\n",timeuse / 1000);


    gettimeofday(&tpstart,NULL);

    /* start knn search, looking for matched descriptors in the database with the minimal distance to those in the source image */
    const int knn = 2;
    flannIndex.knnSearch(despSRC, indices, dists, knn, flann::SearchParams());

    /* 
        @ despSRC query Mat
        @ para indices 
        @ para dists
    */
      
     /* find best match img */
    float nndrRatio = 0.8f;
    vector<ImgFreq>imgFreq;
    for (int i = 0; i < indices.rows; i++){
        if (dists.at<float>(i, 0) < dists.at<float>(i, 1) * nndrRatio){
 
            const int ImgNum = index_IMG.at(indices.at<int>(i, 0));
            bool findImgNum = false;
            for (auto &t : imgFreq){
                if (t.ImgIndex == ImgNum){
                    t.Freq++;
                    findImgNum = true;
                }
 
            }
            if (!findImgNum){
                ImgFreq nextIMG;
                nextIMG.ImgIndex = ImgNum;
                nextIMG.Freq = 1;
                imgFreq.push_back(nextIMG);
            }
        }
    }
   
 
    int maxFreq = 1;
    for (auto &t : imgFreq){
        if (t.Freq > maxFreq)
            maxFreq = t.Freq;
    }

    /* if max matched times is smaller than 3, it fails to find a matched object */    
   
    for (auto &t : imgFreq){ 
        if (t.Freq == maxFreq)
            matchedImgIndex = t.ImgIndex;
    }

    if (matchedImgIndex != 0) {
        if (maxFreq < .02*dscCnt_IMG.at(matchedImgIndex-1)){
            // cout << "Do not find matched ojbect" <<oendl;
            printf("Match at %d, failed with Freq = %d\n",matchedImgIndex,maxFreq);
            matchedImgIndex=0;
            return;
        }
    }
    if (deb) printf("Matched Freq = %d\n",maxFreq);
    if (deb) printf("Matched Image Index = %d\n",matchedImgIndex);

}
 
void ImgMatch::set_minHessian(int m){
    if (m > 0){
        this->minHessian = m;
    }else
        cout << "error:minHessian should bigger than 0!" << endl;
}
 
void ImgMatch::set_dbSize(int s){
    if (s > 0){
        size_DB = s;
    }else
        cout << "error: Database Size should bigger than 0!" << endl;
}

int ImgMatch::getMatchedImgIndex(){
    return this->matchedImgIndex;
}

vector<float> ImgMatch::calLocation(){
  
  if( matchedImgIndex == 0)
    return matchedLocation;
    
    char filename[100];
    sprintf(filename, "/demo/img/%d.jpg", matchedImgIndex);
    Mat matchImg = imread(filename);

    int startPosMatchedDescriptor= 0;
    int endPosMatchedDescriptor = 0; 
    if (matchedImgIndex != 1){
    for( int i = 0; i < matchedImgIndex - 1; i++){

      startPosMatchedDescriptor += dscCnt_IMG.at(i);
    }
        endPosMatchedDescriptor = startPosMatchedDescriptor + dscCnt_IMG.at(matchedImgIndex -1 ) - 1;
    }else{
        endPosMatchedDescriptor = dscCnt_IMG.at(0) - 1;
    }
      /* find descriptors of matchedImg from featureCluster and put into despDB */

      Mat *matchImgDesp = new Mat (endPosMatchedDescriptor - startPosMatchedDescriptor + 1, 64, 5);

      int rowMatchImgDesp = 0;
      for (int i = startPosMatchedDescriptor; i <= endPosMatchedDescriptor; i++){
        featureCluster.row(i).copyTo(matchImgDesp->row(rowMatchImgDesp));
        rowMatchImgDesp++;
      }



    FlannBasedMatcher matcher;
    vector<DMatch>matches;
    // -- Rematch matched image in database with source image to check their matched descriptor
    matcher.match(*matchImgDesp, despSRC, matches);
 
 
    double max_dist = 0; double min_dist = 100;
 
    //-- Quick calculation of max and min distances between keypoints
    for (int i = 0; i < matchImgDesp->rows; i++)
    {
        double dist = matches[i].distance;
        if (dist < min_dist) min_dist = dist;
        if (dist > max_dist) max_dist = dist;
    }
    delete matchImgDesp;
    //-- Draw only "good" matches (i.e. whose distance is less than 3*min_dist )
    std::vector< DMatch > good_matches;
 
    for (int i = 0; i < matchImgDesp.rows; i++)
    {
        if (matches[i].distance < 3 * min_dist)
        {
            good_matches.push_back(matches[i]);
        }
    }
    /*
        \test size of good_matches, if it is smaller than 4, then it fails to draw an outline of a mathed object
    */
    if(deb)printf("number of good match is%zu\n",good_matches.size());

    if (good_matches.size() < 4) 
        return matchedLocation;
 
    Mat img_matches = srcImg;
     
    //-- Localize the object
    vector<Point2f> obj;
    vector<Point2f> scene;
 
    for (uint i = 0; i < good_matches.size(); i++)
    {
        //-- Get the keypoints from the good matches
        obj.push_back(keyPoints2[good_matches[i].queryIdx].pt);
        scene.push_back(keyPoints1[good_matches[i].trainIdx].pt);
    }
 
    Mat H = findHomography(obj, scene, CV_RANSAC);
 
    //-- Get the corners from the matchImg ( the object to be "detected" )
    vector<Point2f> obj_corners(4);
    obj_corners[0] = cvPoint(0, 0); obj_corners[1] = cvPoint(matchImg.cols, 0);
    obj_corners[2] = cvPoint(matchImg.cols, matchImg.rows); obj_corners[3] = cvPoint(0, matchImg.rows);
    vector<Point2f> scene_corners(4);
 
    perspectiveTransform(obj_corners, scene_corners, H);
 
    
    if (matchedLocation.size() >= 8)
    {
        matchedLocation.clear();
    }
    matchedLocation.push_back(scene_corners[0].x);
    matchedLocation.push_back(scene_corners[0].y);
    matchedLocation.push_back(scene_corners[1].x);
    matchedLocation.push_back(scene_corners[1].y);
    matchedLocation.push_back(scene_corners[2].x);
    matchedLocation.push_back(scene_corners[2].y);
    matchedLocation.push_back(scene_corners[3].x);
    matchedLocation.push_back(scene_corners[3].y);
    matchedLocation.push_back(scene_corners[0].x + (scene_corners[1].x - scene_corners[0].x) / 2);
    matchedLocation.push_back(scene_corners[0].y + (scene_corners[3].y - scene_corners[0].y) / 2);
    matchedLocation.push_back(sqrt(((scene_corners[1].x - scene_corners[0].x) / 2)*((scene_corners[1].x - scene_corners[0].x) / 2) + ((scene_corners[3].y - scene_corners[0].y) / 2)* ((scene_corners[3].y - scene_corners[0].y) / 2)));
 
    return matchedLocation;
     
}
 
void ImgMatch::locateDrawRect(vector<float> matchedLocation){
    /*
    \when matchedImgIndex equals to 0, it fails to find a object
    */
    if (deb)
        printf("size of Locating Information%zu",matchedLocation.size());
    if (matchedImgIndex == 0){
        return;
    }

    line(srcImg, cvPoint(matchedLocation.at(0), matchedLocation.at(1)), cvPoint(matchedLocation.at(2), matchedLocation.at(3)), Scalar(0, 0, 0), 2);
    line(srcImg, cvPoint(matchedLocation.at(2), matchedLocation.at(3)), cvPoint(matchedLocation.at(4), matchedLocation.at(5)), Scalar(0, 0, 0), 2);
    line(srcImg, cvPoint(matchedLocation.at(4), matchedLocation.at(5)), cvPoint(matchedLocation.at(6), matchedLocation.at(7)), Scalar(0, 0, 0), 2);
    line(srcImg, cvPoint(matchedLocation.at(6), matchedLocation.at(7)), cvPoint(matchedLocation.at(0), matchedLocation.at(1)), Scalar(0, 0, 0), 2);
    imshow("matched image", srcImg);
    moveWindow("matched image", 600, 350 );
    waitKey();
    clearLocation();
}
 
void ImgMatch::locateDrawCirle(vector<float> matchedLocation){
    circle(srcImg, cvPoint(matchedLocation.at(8), matchedLocation.at(9)), matchedLocation.at(10), Scalar(0, 0, 0), 2, 0);
    imshow("matched image", srcImg);
    waitKey();
}

void ImgMatch::clearLocation(){
    matchedLocation.clear();
}

void ImgMatch::getMatchedImgInfo(){
    string add = imgInfoAdd;
    char infoAdd[100];
    int index =  ceil(matchedImgIndex / double(2));
    cout << "matchedImgIndex is: " << matchedImgIndex << endl;
    cout << "info index is: " << index;
    sprintf(infoAdd, "INFO_%d.txt", index);
    add.append(string(infoAdd));
    ifstream inFile(add);
    string info;
    while (inFile >> info){
        cout << info;
    }
 
}

string ImgMatch::getInfo() {
    string line;
    string info;
    char file_name[100];
    sprintf(file_name, "/demo/info/%d.txt", matchedImgIndex);

    ifstream info_file(file_name);
    if (info_file)
    {
        while (getline(info_file, line)) {
            info = line;
            // cout << line << endl;
        }
    }
    info_file.close();

    return info;
}


string ImgMatch::getInfo(int index) {
    string line;
    string info;
    char file_name[100];
    sprintf(file_name, "/demo/info/%d.txt", index);

    ifstream info_file(file_name);
    if (info_file)
    {
        while (getline(info_file, line)) {
            info = line;
            // cout << line << endl;
        }
    }
    info_file.close();

    return info;
}
