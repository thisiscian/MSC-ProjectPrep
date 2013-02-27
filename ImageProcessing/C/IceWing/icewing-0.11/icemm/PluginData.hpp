/* 
 * File:   PluginData.hpp
 * Author: iluetkeb
 *
 * Created on 30. September 2008, 13:09
 */

#ifndef _PLUGINDATA_HPP
#define	_PLUGINDATA_HPP

#include <string>
#include <main/plugin_comm.h>

namespace ICEWING {
    // do iceWing reference counting using C++ ctor/dtor
    class PluginData {
    protected:
        plugData* data;
    public:
        PluginData() : data(NULL) {
        }
        PluginData(plugData* pd) : data(pd) {
            plug_data_ref(data);
        };

        PluginData(const PluginData& o) : data(o.data) {
            plug_data_ref(data);
        }
        ~PluginData() {
            plug_data_unget(data);
            data = NULL;
        }

        std::string id() const {
            return std::string(this->get()->ident);
        }
        
        plugData& operator *() {
            return *data;
        }
        const plugData& operator* () const {
            return *data;
        }
        plugData* operator ->() {
            return data;
        }
        const plugData* operator ->() const {
            return data;
        }
        
        plugData* get() {
            return data;
        }
        const plugData* get() const {
            return data;
        }
        
        operator bool() const {
            return data != NULL;
        }
        bool operator !() const {
            return data == NULL;
        }
        
        PluginData& operator=(const PluginData& o) {
            if(data != NULL)
                plug_data_unget(data);
            data = o.data;
            plug_data_ref(data);
            return *this;
        }
    };
} // namespace ICEWING

#endif	/* _PLUGINDATA_HPP */

