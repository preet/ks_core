#include <ks/thirdparty/catch/catch.hpp>

#include <ks/KsProperty.h>
#include <ks/KsSignal.h>

using namespace ks;

bool ContainsProperty(std::vector<PropertyBase*> list_props,
                      PropertyBase* prop)
{
    auto it = std::find(
                list_props.begin(),
                list_props.end(),
                prop);

    return(it != list_props.end());
}

TEST_CASE("Properties","[property]")
{
    SECTION("Construction")
    {
        // Construct with value
        Property<uint> width{5};
        Property<uint> height{"height",6}; // +name
        REQUIRE(width.Get() == 5);
        REQUIRE(height.Get() == 6);
        REQUIRE(height.GetName() == "height");

        // Construct with binding (no notifier)
        Property<uint> perimeter {
            [&](){ return 2*width.Get() + 2*height.Get(); } // binding func
        };
        REQUIRE(perimeter.Get() == 22);
        REQUIRE(perimeter.GetInputs().size()==2);
        REQUIRE(perimeter.GetOutputs().size()==0);

        REQUIRE(width.GetOutputs().size()==1);
        REQUIRE(height.GetOutputs().size()==1);

        // binding+name
        Property<uint> half_perimeter {
            "half perimeter",
            [&](){ return (perimeter.Get()*2); }
        };

        // Construct with binding and notifier
        Signal<uint> SignalArea0;
        Property<uint> area0 {
            [&](){ return width.Get()*height.Get(); }, // binding func
            [&](uint const &a){ SignalArea0.Emit(a); } // notifier func
        };

        Signal<> SignalArea1;
        Property<uint> area1 { //+name
            "area1",
            [&](){ return width.Get()*height.Get(); }, // binding func
            [&](uint const &){ SignalArea1.Emit(); } // notifier func
        };
        REQUIRE(area0.Get() == 30);
        REQUIRE(area0.GetInputs().size() == 2);

        REQUIRE(area1.Get() == 30);
        REQUIRE(area1.GetInputs().size() == 2);

        REQUIRE(width.GetOutputs().size()==3);
        REQUIRE(height.GetOutputs().size()==3);
    }

    SECTION("Construction / ReadOnly")
    {
        // Construct with value
        Property<uint,ReadOnly> width{5};
        Property<uint,ReadOnly> height{"height",6}; // +name
        REQUIRE(width.Get() == 5);
        REQUIRE(height.Get() == 6);
        REQUIRE(height.GetName() == "height");

        // Construct with binding (no notifier)
        Property<uint,ReadOnly> perimeter {
            [&](){ return 2*width.Get() + 2*height.Get(); } // binding func
        };
        REQUIRE(perimeter.Get() == 22);
        REQUIRE(perimeter.GetInputs().size()==2);
        REQUIRE(perimeter.GetOutputs().size()==0);

        REQUIRE(width.GetOutputs().size()==1);
        REQUIRE(height.GetOutputs().size()==1);

        // binding+name
        Property<uint,ReadOnly> half_perimeter {
            "half perimeter",
            [&](){ return (perimeter.Get()*2); }
        };

        // Construct with binding and notifier
        Signal<uint> SignalArea0;
        Property<uint,ReadOnly> area0 {
            [&](){ return width.Get()*height.Get(); }, // binding func
            [&](uint const &a){ SignalArea0.Emit(a); } // notifier func
        };

        Signal<> SignalArea1;
        Property<uint,ReadOnly> area1 { //+name
            "area1",
            [&](){ return width.Get()*height.Get(); }, // binding func
            [&](uint const &){ SignalArea1.Emit(); } // notifier func
        };
        REQUIRE(area0.Get() == 30);
        REQUIRE(area0.GetInputs().size() == 2);

        REQUIRE(area1.Get() == 30);
        REQUIRE(area1.GetInputs().size() == 2);

        REQUIRE(width.GetOutputs().size()==3);
        REQUIRE(height.GetOutputs().size()==3);
    }

    SECTION("Destruction")
    {
        Property<uint> width{4};
        Property<uint> height{6};

        // Test destruction of a dependency
        {
            Property<uint> area{
                [&](){ return width.Get()*height.Get(); }
            };
            REQUIRE(width.GetOutputs().size() == 1);
            REQUIRE(height.GetOutputs().size() == 1);
            REQUIRE(area.GetInputs().size()==2);
        }

        REQUIRE(width.GetOutputs().size() == 0);
        REQUIRE(height.GetOutputs().size() == 0);

        Property<uint> perimeter{
            [&](){ return 2*width.Get() + 2*height.Get(); }
        };
        REQUIRE(width.GetOutputs().size() == 1);
        REQUIRE(height.GetOutputs().size() == 1);
        REQUIRE(perimeter.GetInputs().size() == 2);

        // Test destruction of an input
        {
            Property<uint> halfwidth{1};
            width.Bind({
                [&](){ return halfwidth.Get()*2; }
            });
            REQUIRE(halfwidth.GetOutputs().size() == 1);
            REQUIRE(width.Get() == 2);
            REQUIRE(width.GetInputs().size() == 1);
            REQUIRE(width.GetOutputs().size() == 1);
            REQUIRE(height.GetOutputs().size() == 1);
            REQUIRE(perimeter.GetInputs().size() == 2);
            REQUIRE(perimeter.Get() == 16);
        }

        REQUIRE(width.GetInputs().size() == 0);
        REQUIRE(width.GetBindingValid() == false);
        REQUIRE(width.Get() == 2); // expect prev values to stay
        REQUIRE(perimeter.Get() == 16); // // expect prev values to stay
        REQUIRE(width.GetOutputs().size() == 1);
        REQUIRE(height.GetOutputs().size() == 1);
        REQUIRE(perimeter.GetInputs().size() == 2);

        width.Assign(5);
        REQUIRE(perimeter.Get() == 22);
    }

    SECTION("Assignment")
    {
        // Assign Value
        Property<double> meters;
        meters.Assign(3.3);
        REQUIRE(meters.Get() == 3.3);

        // Assign Binding
        Property<double> cm {
            [&](){ return meters.Get()*100.0; }
        };
        REQUIRE(meters.GetOutputs().size() == 1);
        REQUIRE(cm.GetInputs().size() == 1);

        Property<double> mm {
            [&](){ return cm.Get()*10.0; }
        };
        REQUIRE(cm.GetOutputs().size() == 1);
        REQUIRE(mm.GetInputs().size() == 1);

        Property<double> um {
            [&](){ return mm.Get()*1000.0; }
        };
        REQUIRE(mm.GetOutputs().size() == 1);
        REQUIRE(mm.GetInputs().size() == 1);

        SECTION("Bind with duplicate inputs")
        {
            Property<double> cm3 {
                [&](){ return cm.Get()*cm.Get()*cm.Get(); }
            };
            REQUIRE(cm.GetOutputs().size() == 2);
            REQUIRE(cm3.GetInputs().size() == 1);
            REQUIRE(cm3.Get() == (330.*330.0*330.0));
        }

        SECTION("Assign values to property with both Is and Os")
        {
            cm.Assign(5.0);
            // inputs must be cleared
            REQUIRE(cm.GetInputs().size() == 0);
            REQUIRE(cm.GetBindingValid() == false);

            // new value must be assigned
            REQUIRE(cm.Get() == 5.0);

            // outputs must be intact
            REQUIRE(cm.GetOutputs().size() == 1);

            // outputs must be updated
            REQUIRE(mm.Get() == 50.0);
            REQUIRE(um.Get() == 50000);
        }

        SECTION("Change bindings of property with both Is and Os")
        {
            Property<double> err_val{0.0};

            cm.Bind({
                [&](){ return meters.Get()*100.0 + err_val.Get(); }
            });

            // inputs must be captured
            REQUIRE(cm.GetInputs().size() == 2);

            // new value must be evaluated
            REQUIRE(cm.Get() == 330);

            // outputs must be intact
            REQUIRE(cm.GetOutputs().size() == 1);

            // outputs must be updated
            REQUIRE(mm.Get() == 3300);
            REQUIRE(um.Get() == 3300000);
        }

        SECTION("Verify sequential binding assignment")
        {
            Property<double> width{1};
            Property<double> height{2};
            Property<double> depth{3};
            Property<double> volume{
                [&](){ return(width.Get()*height.Get()*depth.Get());  }
            };

            REQUIRE(ContainsProperty(volume.GetInputs(),&width));
            REQUIRE(ContainsProperty(volume.GetInputs(),&height));
            REQUIRE(ContainsProperty(volume.GetInputs(),&depth));

            Property<double> radius{4};

            volume.Bind({
                [&](){
                    double const r = radius.Get();
                    return (4.0/3.0) * (3.1416) * (r*r*r);
                }
            });

            REQUIRE(volume.GetInputs().size() == 1);
            REQUIRE(ContainsProperty(volume.GetInputs(),&radius));
        }
    }

    SECTION("Redundant property changes")
    {
        // Triangle with x,y,hyp,p(perimeter):
        {
            Property<double> x{2};
            Property<double> y{4};

            uint hyp_eval_count=0;
            Property<double> hyp{
                [&](){
                    hyp_eval_count++;
                    return sqrt(x.Get()*x.Get() + y.Get()*y.Get());
                }
            };
            REQUIRE(hyp_eval_count==1);

            uint p_eval_count=0;
            Property<double> p{
                [&](){
                    p_eval_count++;
                    return (x.Get()+y.Get()+hyp.Get());
                }
            };
            REQUIRE(p_eval_count==1);

            // A naive evaluation would result in p being
            // updated twice but we do a topo sort before
            // evaluating:
            x.Assign(3);
            REQUIRE(hyp_eval_count==2);
            REQUIRE(p_eval_count==2);
        }

        // Circuit with v(volts),i(amps),r0(ohms),r1,r2,
        // d0,d1,d2(d==voltage drops in volts across resistors)
        {
            Property<double> v{12.0};
            Property<double> r0{50.0};
            Property<double> r1{100.0};
            Property<double> r2{200.0};

            uint i_eval_count=0;
            Property<double> i{
                [&](){
                    i_eval_count++;
                    return ((r0.Get()+r1.Get()+r2.Get())/v.Get());
                }
            };
            REQUIRE(i_eval_count == 1);

            uint d0_eval_count=0;
            Property<double> d0{
                [&](){
                    d0_eval_count++;
                    return i.Get()*r0.Get();
                }
            };
            REQUIRE(d0_eval_count == 1);

            uint d1_eval_count=0;
            Property<double> d1{
                [&](){
                    d1_eval_count++;
                    return i.Get()*r1.Get();
                }
            };

            uint d2_eval_count=0;
            Property<double> d2{
                [&](){
                    d2_eval_count++;
                    return i.Get()*r2.Get();
                }
            };

            // Naive evaluation would update d0_eval_count twice
            // but with topo sort it we should only need one
            // (1 original binding update + 1 as a result of
            // a new assignment == 2)
            r0.Assign(100.0);
            REQUIRE(i_eval_count == 2);
            REQUIRE(d0_eval_count == 2);
        }
    }

    SECTION("Glitches")
    {
        // (This case is pretty much covered by the section on
        // Redundant changes, but just for clarity)

        // A glitch is a spurious incorrect value of a
        // property while updates are being propagated
        // ex:
        // a = 1;
        // b = a*1;
        // c = a+b;

        // Naive:
        // a.Assign(2): a==2, b(not updated yet)==1, c==(2+1)==3

        // topo sort should prevent this since b and c
        // will only be evaluated once in the correct
        // order after a is changed
        Property<uint> a{1};

        Property<uint> b{
            [&](){
                return a.Get()*1;
            }
        };

        std::vector<uint> c_values;

        Property<uint> c{
            [&](){
                uint val = a.Get()+b.Get();
                c_values.push_back(val);
                return val;
            }
        };

        a.Assign(2);
        REQUIRE(c_values.size() == 2);
        REQUIRE(c_values.front() == 2);
        REQUIRE(c_values.back() == 4);  // should never be 3
    }

    SECTION("Binding loops")
    {
        // Check for a self loop where a Property
        // refers to itself
        LOG.Info() << "Expect warning about property using "
                      "itself as a dependency:";
        Property<uint> x;
        x.Bind({
            [&](){ return x.Get() + 1; }
        });
        REQUIRE(x.GetInputs().size() == 0);
        REQUIRE(x.GetOutputs().size() == 0);
        REQUIRE(x.GetBindingValid() == false);

        // Binding loop without self dependencies
        // b,c,d form a loop
        LOG.Info() << "Expect warning about binding loop:";
        Property<uint> a{"a",1};
        Property<uint> d{"d",1};
        Property<uint> b{"b", [&](){ return a.Get() + d.Get(); } };
        Property<uint> c{"c", [&](){ return b.Get()*1; } };
        d.Bind({ [&](){ return c.Get()*1; } });
    }

    SECTION("Thread local properties")
    {
        // The test results are collected and then verified
        // serially at the end because Catch is not thread safe:
        // https://github.com/philsquared/Catch/issues/99

        std::mutex m;
        std::vector<bool> list_all_results;

        auto test0 = [&]() {
            Property<double> v{12.0};
            Property<double> r0{50.0};
            Property<double> r1{100.0};
            Property<double> r2{200.0};

            Property<double> i{
                [&](){ return ((r0.Get()+r1.Get()+r2.Get())/v.Get()); }
            };

            Property<double> d0{
                [&](){ return i.Get()*r0.Get(); }
            };

            Property<double> d1{
                [&](){ return i.Get()*r1.Get(); }
            };

            Property<double> d2{
                [&](){ return i.Get()*r2.Get(); }
            };

            std::vector<bool> list_results;

            for(double alt=12.0; alt < 14.0; alt+=0.25) {
                v.Assign(alt);
                double const i_val = 350.0/alt;
                list_results.push_back(i.Get() == i_val);
            }

            m.lock();
            list_all_results.insert(
                        list_all_results.begin(),
                        list_results.begin(),
                        list_results.end());
            m.unlock();
        };

        auto test1 = [&]() {
            Property<uint> x{1};
            Property<uint> y{2};
            Property<uint> perimeter {
                [&](){ return 2*(x.Get()+y.Get()); }
            };
            Property<uint> area {
                [&](){ return x.Get()*y.Get(); }
            };

            std::vector<bool> list_results;
            for(uint i=2; i < 10; i++) {
                x.Assign(i);
                uint const p = 2*(x.Get()+y.Get());
                uint const a = x.Get()*y.Get();
                bool p_ok = (p == perimeter.Get());
                bool a_ok = (a == area.Get());
                list_results.push_back(p_ok&&a_ok);
            }

            m.lock();
            list_all_results.insert(
                        list_all_results.begin(),
                        list_results.begin(),
                        list_results.end());
            m.unlock();
        };

        std::vector<std::thread> list_threads;
        for(uint i=0; i < 8; i++) {
            if(i%2==0) {
                std::thread thread(test0);
                list_threads.push_back(std::move(thread));
            }
            else {
                std::thread thread(test1);
                list_threads.push_back(std::move(thread));
            }
        }
        for(auto &t : list_threads) {
            t.join();
        }

        bool all_ok=true;
        for(auto ok : list_all_results) {
            all_ok = all_ok && ok;
        }

        REQUIRE(all_ok);
    }
}
