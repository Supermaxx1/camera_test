std::cout << "╔═══════════════════════════════════╗" << std::endl;
    std::cout << "║  LIVE SCORING SYSTEM - 10m RIFLE  ║" << std::endl;
    std::cout << "╠═══════════════════════════════════╣" << std::endl;
    std::cout << "║  Press SPACE to score current shot║" << std::endl;
    std::cout << "║  Press 'r' to reset template       ║" << std::endl;
    std::cout << "║  Press ESC to exit                 ║" << std::endl;
    std::cout << "╚═══════════════════════════════════╝\n" << std::endl;
    
    cv::Mat frame, deskewed, visual;
    int shotCount = 0;
    
    while(true) {
        cap >> frame;
        if(frame.empty()) break;
        
        deskewed = deskewImage(frame, config["skew"]);
        cv::imshow("Live View", deskewed);
        
        int key = cv::waitKey(30);
        
        if(key == 27) break; // ESC
        
        if(key == ' ') { // SPACE - score shot
            cv::Point2f holePos = detectHole(templateClean, deskewed, visual);
            
            if(holePos.x >= 0) {
                shotCount++;
                float dx = holePos.x - 400.0f;
                float dy = holePos.y - 400.0f;
                float distPx = std::sqrt(dx*dx + dy*dy);
                float distMM = distPx / pxPerMM;
                double score = computeScoreRifle(distMM);
                
                std::cout << "\n╔═══════════════════════════════════╗" << std::endl;
                std::cout << "║  SHOT #" << std::setw(2) << shotCount << "                        ║" << std::endl;
                std::cout << "╠═══════════════════════════════════╣" << std::endl;
                std::cout << "║  Position: (" << std::fixed << std::setprecision(1) 
                          << std::setw(5) << holePos.x << ", " << std::setw(5) << holePos.y << ")     ║" << std::endl;
                std::cout << "║  Distance: " << std::setw(5) << distMM << " mm              ║" << std::endl;
                std::cout << "║  SCORE:    " << std::setw(4) << score << "                   ║" << std::endl;
                std::cout << "╚═══════════════════════════════════╝" << std::endl;
                
                cv::imshow("Shot Result", visual);
                
                // Update template (now includes this hole)
                templateClean = deskewed.clone();
            } else {
                std::cout << "No new hole detected. Try again." << std::endl;
            }
        }
        
        if(key == 'r' || key == 'R') { // Reset template
            templateClean = deskewed.clone();
            std::cout << "Template reset!" << std::endl;
        }
    }
    
    std::cout << "\nSession ended. Total shots: " << shotCount << std::endl;
    return 0;
}
