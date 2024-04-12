/*
    Copyright (C) 2019-2024 Noisetron LLC, Bert Schiettecatte 

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "DboStore.h" 

int main() { 

	DboBook b;
	
	b.addOrUpdate(osAsk, 12000.00, 10.0, 0, 0, 0, "A2B2", 0, true, utNewOrChanged); 
	b.addOrUpdate(osAsk, 12000.25, 77.0, 0, 0, 0, "1FF3", 0, true, utNewOrChanged); 
	b.addOrUpdate(osAsk, 12000.25, 77.0, 0, 0, 0, "1FF3", 0, true, utDeleted); 
	b.addOrUpdate(osAsk, 12001.00, 11.0, 0, 0, 0, "1XFY", 0, true, utNewOrChanged); 
	b.addOrUpdate(osAsk, 12000.25, 22.0, 0, 0, 0, "X5FY", 0, true, utNewOrChanged); 
	b.addOrUpdate(osAsk, 12000.25, 52.0, 0, 0, 0, "X5FY", 0, true, utNewOrChanged); 

	b.addOrUpdate(osBid, 11999.75, 10.0, 0, 0, 0, "2XGZ", 0, true, utNewOrChanged); 
	b.addOrUpdate(osBid, 11999.50, 50.0, 0, 0, 0, "23GH", 0, true, utNewOrChanged); 
	b.addOrUpdate(osBid, 11998.00, 20.0, 0, 0, 0, "AC12", 0, true, utNewOrChanged); 

	std::map<PriceType, double> askMap; 
	std::map<PriceType, double> bidMap; 

	std::cout << std::endl; 
	b.getTopEntries(osAsk, 10, askMap); 
	for (auto ask: askMap) 
		std::cout << "\t" << ask.first << "\t\t" << ask.second << std::endl; 

	std::cout << std::endl; 
	b.getTopEntries(osBid, 10, bidMap);  
	for (auto bid: bidMap) 
		std::cout << "\t" << bid.first << "\t\t" << bid.second << std::endl; 

	std::cout << std::endl; 
	b.printOrders(osAsk, 100); 
	std::cout << std::endl; 
	b.printOrders(osBid, 100); 

	std::cout << std::endl;
	b.printTopAsks(2); 
	std::cout << std::endl;
	b.printTopBids(2); 

	return EXIT_SUCCESS; 
}

