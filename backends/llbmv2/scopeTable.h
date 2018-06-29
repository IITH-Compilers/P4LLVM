/*
IIT Hyderabad

authors:
Venkata Keerthy, Pankaj K, Bhanu Prakash T, D Tharun Kumar
{cs17mtech11018, cs15btech11029, cs15btech11037, cs15mtech11002}@iith.ac.in

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/
#ifndef _SCOPE_TABLE_H_
#define _SCOPE_TABLE_H_

#include <map>
#include <iterator>
#include <vector>
#include "iostream"

template <typename T>
class ScopeTable{
    int scope;
    std::vector<std::map<std::string, T>> dict;
    typename std::vector<std::map<std::string, T>>::iterator it;

  public:
    ScopeTable(){
        scope = 0;
        dict.push_back(std::map<std::string,T>());
    }

    void insert(std::string str, T t)   {
        dict.at(scope).insert(std::pair<std::string,T>(str,t));
    }

    void enterScope()   {
        scope++;
        dict.push_back(std::map<std::string,T>());        
    }

    void exitScope()    {
        if(scope>0) {
            it = dict.begin();         
            dict.erase(it+scope);
            scope--;
        }
    }

    T lookupLocal(std::string label) {
        auto entry = dict.at(scope).find(label);
        typename std::map <std::string, T> map = dict.at(scope);
        if(entry != dict.at(scope).end())
            return entry->second;
        else
            return 0;

    }

    T lookupGlobal(std::string label)   {
        for(int i=scope; i>=0; i--) {
            auto entry = dict.at(i).find(label);
            if(entry != dict.at(i).end())   {
                return entry->second;
            }
        }
        return 0;
    }

    void printAll() {           
        for(auto vitr = dict.begin(); vitr!=dict.end();vitr++) {
            typename std::map <std::string, T> map(vitr->begin(), vitr->end());
            // std::cout<<"\nmap\n--------\n";
            for(auto mitr = map.begin(); mitr!=map.end(); mitr++)   {
                // std::cout<<"\n" << mitr->first << "\t-\t" << mitr->second <<"\n";
            }
            
        }
    }

    int getCurrentScope() {
        return scope;
    }

    std::map<std::string, T>& getVars(int sc) {
        assert(sc>=0 && sc<=scope);
        return dict.at(sc);
    }
};

#endif 