#!/bin/bash

# Set up environment variables
source "$INIT_ENV"
source "$ROS_2_ENV" 

# Define packages to exclude
EXCLUDE_PACKAGES="approximate_intersection"

# Find all packages
PACKAGES=$(find . -maxdepth 2 -type f -name package.xml | sed 's/\.\///' | cut -d/ -f1)

# Filter out excluded packages
PACKAGES_TO_BUILD=$(echo "$PACKAGES" | tr ' ' '\n' | grep -vE "$EXCLUDE_PACKAGES" | tr '\n' ' ')

# Modify the build command
sed -i "/colcon build/ s/\$/ --parallel-workers 4 --packages-up-to $PACKAGES_TO_BUILD/" /home/carma/.ci-image/engineering_tools/code_coverage/make_with_coverage.bash

# Loop through packages and build them individually
for package in $PACKAGES_TO_BUILD; do
    echo "Building package: $package"
    make_with_coverage.bash -m -e /opt/carma/ -o ./coverage_reports/gcov $package

    # Check if the build failed
    if [ $? -ne 0 ]; then
        echo "Package $package build failed but continuing..."
    else
        echo "Package $package build succeeded."
    fi
done
