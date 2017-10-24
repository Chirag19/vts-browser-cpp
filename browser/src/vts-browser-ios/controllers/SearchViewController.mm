/**
 * Copyright (c) 2017 Melown Technologies SE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * *  Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "../Map.h"
#include <vts-browser/search.hpp>

#import "SearchCell.h"
#import "SearchViewController.h"


@interface SearchViewController ()
{
	std::shared_ptr<vts::SearchTask> task;
	std::shared_ptr<vts::SearchTask> result;
}

@property (weak, nonatomic) IBOutlet UISearchBar *searchBar;
@property (weak, nonatomic) IBOutlet UITableView *tableView;
@property (weak, nonatomic) IBOutlet UIActivityIndicatorView *activityIndicator;

@end


@implementation SearchViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    mapTimerStart(self, @selector(timerTick));
    [_searchBar becomeFirstResponder];
}

- (void)timerTick
{
	if (task && task->done)
		[_activityIndicator stopAnimating];
	else
		[_activityIndicator startAnimating];

	if (result)
	{
		assert(result->done);
		double pos[3];
		map->getPositionPoint(pos);
		result->updateDistances(pos);
	}
	
	[_tableView reloadData];
}

- (void)searchBar:(UISearchBar*)searchBar textDidChange:(NSString*)searchText
{
	task = map->search([_searchBar.text UTF8String]);
}

- (void)searchBarSearchButtonClicked:(UISearchBar *)searchBar
{
	task = map->search([_searchBar.text UTF8String]);
    [_searchBar resignFirstResponder];
}

- (void)searchBarResultsListButtonClicked:(UISearchBar *)searchBar
{
	task = map->search([_searchBar.text UTF8String]);
    [_searchBar resignFirstResponder];
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
	if (!result)
		return;
	assert(result->done);
    vts::SearchItem &item = result->results[indexPath.row];
    map->setPositionSubjective(false, false);
    map->setPositionViewExtent(std::max(6667.0, item.radius * 2), vts::NavigationType::FlyOver);
    map->setPositionRotation({0,270,0}, vts::NavigationType::FlyOver);
    map->resetPositionAltitude();
    map->resetNavigationMode();
    map->setPositionPoint(item.position, vts::NavigationType::FlyOver);
	[self.navigationController popViewControllerAnimated:YES];
}

#pragma mark - Table view data source

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
	if (task && task->done)
		result = task;
	else
		result = nullptr;
	return result ? result->results.size() : 0;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
	assert(result && result->done);
    SearchCell *cell = [tableView dequeueReusableCellWithIdentifier:@"SearchCell" forIndexPath:indexPath];
    vts::SearchItem &item = result->results[indexPath.row];
    cell.cellName1.text = [NSString stringWithUTF8String:item.title.c_str()];
    cell.cellName2.text = [NSString stringWithUTF8String:item.displayName.c_str()];
    if (item.distance >= 1e3)
    	cell.cellDistance.text = [NSString stringWithFormat:@"%.1f km", item.distance / 1e3];
    else
    	cell.cellDistance.text = [NSString stringWithFormat:@"%.1f m", item.distance];
    return cell;
}

@end
