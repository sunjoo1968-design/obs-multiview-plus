local hybrid_original_load = script_load
function script_load(settings)
    obs.obs_data_set_int(settings,'me_count',2)
    for n=1,2 do
        obs.obs_data_set_int(settings,key(n,'setup_count'),2)
        for i=1,2 do
            obs.obs_data_set_string(settings,key(n,'pick_'..i),(n==1 and 'Test Scene ' or 'Test Camera ')..i)
        end
    end
    hybrid_original_load(settings)
    local finished
    finished=function(event)
        if event==obs.OBS_FRONTEND_EVENT_FINISHED_LOADING then
            obs.obs_frontend_remove_event_callback(finished)
            assert(setup_all(),'Hybrid isolated setup failed')
            local function test_mode(mode)
                return function(pressed)
                    if not pressed then return end
                    linked_pending=nil
                    for n=1,me_count do
                        banks[n].mode=mode; banks[n].duration=2000
                        banks[n].pending=nil; banks[n].commit=nil; banks[n].locked_until=0
                    end
                end
            end
            obs.obs_hotkey_register_frontend('sunjoo_obs_link_test_mix','Test only: Link MIX',test_mode('MIX'))
            obs.obs_hotkey_register_frontend('sunjoo_obs_link_test_cut','Test only: Link CUT',test_mode('CUT'))

            local file=assert(io.open(script_path()..'hybrid-ready.txt','wb'));file:write('READY');file:close()
        end
    end
    obs.obs_frontend_add_event_callback(finished)
end
