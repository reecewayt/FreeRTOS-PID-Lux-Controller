# Note: This script is intended to be ran once when first setting up the project. 
# after project setup it is up to the user to ensure any added or deleted files
# adhere to the design flow and are checked into git appropriately. 

# 1. Set paths and project info
# Project name updated for this repository
set proj_name "FreeRTOS_PID_Lux_Controller"
set proj_dir "../build/${proj_name}"
set src_dir "../vivado_src"

# Check if the project directory already exists and delete it
if { [file exists $proj_dir] } {
    file delete -force $proj_dir
}

# Part number for BOTH boards
set part_num "xc7a100tcsg324-1"

# 2. Create the project
create_project ${proj_name} ${proj_dir} -part ${part_num}

# 2a. Add local IP repository so Vivado can find the included custom IP
# This ensures vivado_src/ip/nexys4io_3_0 (and siblings) are visible to the IP catalog
set ip_repo "${src_dir}/ip"
if { [file exists $ip_repo] } {
    # add the repository path to the current project
    set_property ip_repo_paths [list $ip_repo] [current_project]
    # refresh the IP catalog so the new repo is loaded
    update_ip_catalog -rebuild
}

# 3. Add source files (HDL and Block Design)
#    This method checks if files exist before trying to add them.

set hdl_files_v [glob -nocomplain ${src_dir}/hdl/*.v]
if { [llength $hdl_files_v] > 0 } {
    add_files -norecurse $hdl_files_v
}

set hdl_files_sv [glob -nocomplain ${src_dir}/hdl/*.sv]
if { [llength $hdl_files_sv] > 0 } {
    add_files -norecurse $hdl_files_sv
}

set bd_files [glob -nocomplain ${src_dir}/bd/embsys/*.bd]
if { [llength $bd_files] > 0 } {
    add_files -norecurse $bd_files
}

# 4. Add BOTH constraint files (Nexys4 and Nexys A7 names used in this repo)
set constr_nexys4 [glob -nocomplain ${src_dir}/constraints/nexys4fpga.xdc]
if { [llength $constr_nexys4] > 0 } {
    add_files -fileset constrs_1 $constr_nexys4
    set_property USED_IN {synthesis implementation} [get_files $constr_nexys4]
}


set constr_nexys7 [glob -nocomplain ${src_dir}/constraints/nexysA7fpga.xdc]
if { [llength $constr_nexys7] > 0 } {
    add_files -fileset constrs_1 $constr_nexys7
    set_property USED_IN {synthesis implementation} [get_files $constr_nexys7]
    
    # By default, disable the Nexys A7 file.
    set_property IS_ENABLED false [get_files $constr_nexys7]
}

puts "Project created. By default, 'nexys4fpga.xdc' is active."
puts "To use the Nexys A7, open the project in the GUI and enable 'nexysA7fpga.xdc' and disable 'nexys4fpga.xdc'."