/* main.cc
 *
 * Copyright (C) 2002 The libxml++ development team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include <libxml++/libxml++.h>
#include <iostream>
#include <cstdlib>
#include <queue>
#include <stack>
#include <musly/musly.h>
#include <boost/filesystem.hpp>
#include <sstream>
#include <boost/optional.hpp>
#include <cstdio>
#include <memory>
#include <array>
#include <stdexcept>

static double threshold = 10;
static int _cnter = 0;
/* call system command，return the result after running */
std::string exec(const char *cmd)
{
    std::array<char, 512> buffer;
    std::string result;
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe)
        throw std::runtime_error("popen failed");
    while (!feof(pipe.get()))
    {
        if (fgets(buffer.data(), 512, pipe.get()) != NULL)
            result += buffer.data();
    }
    return result;
}

// return bool if the [node] is marked as deleted
bool is_delelted_node(xmlpp::Element &node)
{
    return node.get_attribute("__d") != nullptr;
}

// mark node as deleted
void set_as_deleted(xmlpp::Element &node)
{
    node.set_attribute("__d", "true");
}

// remove the "deleted" mark
void unset_as_deleted(xmlpp::Element &node)
{
    node.remove_attribute("__d");
}

// delete node and the children
void filter_undeleted_nodes(xmlpp::Node &node)
{
    // only delete elements, ignore comments
    auto *pEle = dynamic_cast<xmlpp::Element*>(&node);
    if (!pEle)
        return;
    
    auto pCur = node.get_first_child();
    while (pCur != nullptr)
    { // traverse the node

        auto curEle = dynamic_cast<xmlpp::Element*>(pCur);
        if (curEle == nullptr || !is_delelted_node(*curEle))
        { // if it is not an element or not deleted node, we keep it
            // recursion
            filter_undeleted_nodes(*pCur);
            pCur = pCur->get_next_sibling();
        }
        else
        {
        //remove the element
            auto t = pCur;
            pCur = pCur->get_next_sibling();
            node.remove_node(t);
        }
    }
}

// copy the xml file and return it to parser
xmlpp::DomParser* deeply_copy(xmlpp::Document &doc)
{
    auto str = doc.write_to_string();
    xmlpp::DomParser *parser = new xmlpp::DomParser;
    parser->parse_memory(str);
    return parser;
}

//transfor to wav and save to workdir, return the path
boost::optional<boost::filesystem::path> to_workdir_wav(xmlpp::Document &doc)
{
    
    boost::filesystem::path workdirpath("_work");
    if (_cnter == 0)
    { // create workdirpath if it is first time
        if (boost::filesystem::exists(workdirpath))
            boost::filesystem::remove_all(workdirpath);
        boost::filesystem::create_directories(workdirpath);
    }
    ++_cnter;
    
    // write musicxml into file
    auto tempxmlpath = workdirpath;
    tempxmlpath.append("_" + std::to_string(_cnter) + ".xml");
    doc.write_to_file(tempxmlpath.string());
    
    // use musicxml2mid to convert musicxml to midi
    //perl script musicxml2mid provided by http://www.pjb.com.au/midi/musicxml2mid.html
    auto tempmidipath = tempxmlpath;
    tempmidipath.replace_extension("midi");
    std::ostringstream cmdstr;
    cmdstr << "perl musicxml2mid " << tempxmlpath << " > " << tempmidipath;
    auto xml2midstatus = exec(cmdstr.str().c_str());
    //std::cout << "result: **" << xml2midstatus << "**\n";
    
    // use timidity to convert MIDI into .wav
    auto tempwavpath = tempxmlpath;
    tempwavpath.replace_extension("wav");
    cmdstr = std::ostringstream();
    cmdstr << "timidity " << tempmidipath << " -Ow -o " << tempwavpath;
    auto mid2wavstatus = exec(cmdstr.str().c_str());
    //std::cout << "result: **" << mid2wavstatus << "**\n";
    if (mid2wavstatus.find("Can't read track header") != mid2wavstatus.npos)
    { // if convertion fails output fail message
       // std::cout << "!!Failed\n";
        return {};
    }
    
    // DEBUG
    std::cout << tempxmlpath << "\n" << tempmidipath << "\n" << tempwavpath << "\n";
    
    return tempwavpath;
}

// return if the similarity of sliced wav file with [origTrack] in musly junk box[jb] is less than threshold
bool satisfy(musly_jukebox *jb, musly_track* origTrack, musly_trackid origTrackID, const boost::filesystem::path &testedWav, double threshold)
{
    //std::cout << "Comparing " << testedWav << "...\n";
    // allocate the track
    auto testedTrack = musly_track_alloc(jb);
    // add the tested .wav
    auto jbresult =
    musly_track_analyze_audiofile(
                                  jb, testedWav.string().c_str(), 0, 0, testedTrack);
    if (jbresult)
    {
        std::cerr << "Can not add sound file " << testedWav << " to libmusly.\n";
        return false;
    }
    
    musly_trackid testedTrackID = 0;
    // add track to musly
    jbresult = musly_jukebox_addtracks(
                                       jb, &testedTrack, &testedTrackID, 1, 0);
    
    //std::cout << "The track id of sound file " << testedWav << " is " << origTrackID << "\n";
    
    musly_track *tracks[2];
    tracks[0] = origTrack;
    tracks[1] = testedTrack;
    
    musly_trackid trackIDs[2];
    trackIDs[0] = origTrackID;
    trackIDs[1] = testedTrackID;
    
    musly_jukebox_setmusicstyle(jb, tracks, 2);
    
    float result[10] = {};
    // determine the similarity
    musly_jukebox_similarity(jb, origTrack, origTrackID, &testedTrack, &testedTrackID, 1, result);
    
    for (auto &r : result)
        //std::cout << r << " ";
    //std::cout << "\n";
    
        // remove track
    musly_jukebox_removetracks(jb, &testedTrackID, 1);
    musly_track_free(testedTrack);
    
    if (result[0] < threshold)
        return true;
    return false;
}

int main(int argc, char* argv[])
{
    int total_node_attempt=0;
    int node_deleted = 0;
    
    // initialise musly with mandel-ellis algorithm
    auto jb = musly_jukebox_poweron("mandelellis", "libav");
    if (!jb)
    {
        std::cerr << "jbchucuo";
        return -1;
    }
    
    //std::cout << "jb works\n";
    // Set the global C++ locale to the user-specified locale. Then we can
    // hopefully use std::cout with UTF-8, via Glib::ustring, without exceptions.
    std::locale::global(std::locale(""));
    
    bool validate = false;
    bool set_throw_messages = false;
    bool throw_messages = false;
    bool substitute_entities = true;
    bool include_default_attributes = false;
    
    int argi = 1;
    while (argc > argi && *argv[argi] == '-') // option
    {
        switch (*(argv[argi]+1))
        {
            case 'v':
                validate = true;
                break;
            case 't':
                set_throw_messages = true;
                throw_messages = true;
                break;
            case 'e':
                set_throw_messages = true;
                throw_messages = false;
                break;
            case 'E':
                substitute_entities = false;
                break;
            case 'a':
                include_default_attributes = true;
                break;
            default:
                std::cout << "Usage: " << argv[0] << " [-v] [-t] [-e] [filename]" << std::endl
                << "       -v  Validate" << std::endl
                << "       -t  Throw messages in an exception" << std::endl
                << "       -e  Write messages to stderr" << std::endl
                << "       -E  Do not substitute entities" << std::endl
                << "       -a  Include default attributes in the node tree" << std::endl;
                return EXIT_FAILURE;
        }
        argi++;
    }
    std::string filepath;
    if(argc > argi){
        filepath = argv[argi]; //Allow the user to specify a different XML file to parse.
        threshold = strtod(argv[2],NULL);
    }
    else
        filepath = "1.xml";
    
    try
    {
        xmlpp::DomParser parser;
        if (validate)
            parser.set_validate();
        if (set_throw_messages)
            parser.set_throw_messages(throw_messages);
        //We can have the text resolved/unescaped automatically.
        parser.set_substitute_entities(substitute_entities);
        parser.set_include_default_attributes(include_default_attributes);
        parser.parse_file(filepath);
        
        if(parser)
        {
            // read and convert the original musicxml as [origWav]
            auto pOrigDoc = parser.get_document();
            auto origWav = to_workdir_wav(*pOrigDoc);
            if (!origWav)
            {
                std::cerr << "The input file isn't a valid musicxml file.\n";
                return -1;
            }
            
            // add it into musly, ready for the comparison
            auto origTrack = musly_track_alloc(jb);
            auto jbresult =
            musly_track_analyze_audiofile(
                                          jb, origWav.get().string().c_str(), 0, 0, origTrack);
            if (jbresult)
            {
                std::cerr << "Can not add original sound file to libmusly.\n";
                return -1;
            }
            musly_trackid origTrackID = 0;
            jbresult = musly_jukebox_addtracks(
                                               jb, &origTrack, &origTrackID, 1, 1);
            //std::cout << "The track id of original sound file is " << origTrackID << "\n";
            //return 0;
            
            // the queue starting with root
            std::queue<xmlpp::Element*> processQueue;
            processQueue.push(pOrigDoc->get_root_node());
            
            while (!processQueue.empty())
            {
                // pop a node
                total_node_attempt++;
                auto pElem = processQueue.front();
                processQueue.pop();
                
                // a lambda expression, can be considered as a function
                //http://stackoverflow.com/questions/3832337/interview-what-is-lambda-expression
                // this function put all children nodes of that particular node into the queue

                auto pushChilds = [pElem, &processQueue]()
                {
                    auto child = pElem->get_first_child();
                    while (child != nullptr)
                    {
                        auto elemChild = dynamic_cast<xmlpp::Element*>(child);
                        if (elemChild != nullptr)
                            processQueue.push(elemChild);
                        child = child->get_next_sibling();
                    }
                };
                
                // if it is the root, we cannot delete it. Simply push it child and return
                if (pElem == pOrigDoc->get_root_node())
                {
                    pushChilds();
                    continue;
                }
                
                //std::cout << "\nDeleting " << pElem->get_name() << "...\n";
                //set the node as deleted
                set_as_deleted(*pElem);
                
                // copy current musicxml file
                auto newdocparser = deeply_copy(*pOrigDoc);
                auto newdoc = newdocparser->get_document();
                
                // delete the node marked as deleted for the current file
                filter_undeleted_nodes(*newdoc->get_root_node());
                
                // convert the current musicxml to .wav
                auto curWav = to_workdir_wav(*newdoc);
                
                // compare similarity
                if (!curWav || !satisfy(jb, origTrack, origTrackID, curWav.get(), threshold))
                { // If it cannot meet requirement "threshold"
                    std::cout << "Not a sound file or can not deleted\n";
                    // unset it as deleted
                    unset_as_deleted(*pElem);
                    // push its child into the queue
                    pushChilds();
                }
                else
                    node_deleted++;
                
                delete newdocparser;
            }
            
            filter_undeleted_nodes(*pOrigDoc->get_root_node());
            auto finalWav = to_workdir_wav(*pOrigDoc);
            //calcalating test results
            if (finalWav){
                int numLinesOrig = 0;
                std::string str2 ("<");

                std::ifstream in("_work/_1.xml");
                std::string unused;
                while ( std::getline(in, unused)){
                    if(unused.find(str2) != std::string::npos)
                    ++numLinesOrig;
                }
                       
                std::ifstream inn( "_work/_" + std::to_string(_cnter) + ".xml" );
                std::string buffer;
                std::size_t numLinesFinal;
                

                while ( getline( inn , buffer )&& !buffer.length()==0)
                {
                    if(buffer.find(str2) != std::string::npos)
                        ++numLinesFinal;
                }
                //output the test result
                std::cout << "Original file counter: " << numLinesOrig<< "\n";
                std::cout << "Threshold:  " << threshold <<"\n";
                std::cout << "Final wave file is " << finalWav.get() << "\n";
                std::cout << "Final xml file's line counter " << numLinesFinal << "\n";
                std::cout << "Number of Node deleted:  " << node_deleted << "\n";
                std::cout << "Number of Node attempted:  " << total_node_attempt << "\nRuntime:";

                
               // std::cout<<"Time taken:"<< (double)(clock() - tStart)/CLOCKS_PER_SEC;
            }
            else
                std::cerr << "Error! Cannot generate final wave.\n";
            // End NEG edits
        }
    }

    catch(const std::exception& ex)
    {
        std::cerr << "Exception caught: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;

}
